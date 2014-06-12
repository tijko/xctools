/*
 * Copyright (c) 2014 Citrix Systems, Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <dbus/dbus-glib-lowlevel.h>
#include <glib-object.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

#ifdef LIBAPPINDICATOR
#include <libappindicator/app-indicator.h>
#endif

#include <dbus/dbus.h>

#include "strbuf.h"
#include "util.h"

#define XENMGR "com.citrix.xenclient.xenmgr"

#define APP_NAME "xc-switcher"
#define NOTIFY_TIMEOUT 60000

#define DATA_DIR "/usr/share/xc-switcher/"

/* #define DEBUG 1 */

struct app_state {
    DBusConnection* conn;
    DBusError *err;
    struct strbuf **doms;

    GMainLoop* loop;
#ifdef LIBAPPINDICATOR
    AppIndicator *ind;
#else
    GtkStatusIcon *ind;
#endif

    GtkWidget *menu;

    struct strbuf vm_path;
};

static struct app_state app;

int refresh_menu(void);
struct strbuf *udbus_msg_strings(DBusMessage *message, int number);
dbus_uint32_t udbus_get_uint32(char *dest, char *path, char *inf, char *method);
struct strbuf udbus_get_sval(DBusMessage *msg);

static void notify(const char* summary, const char* body,
                   NotifyUrgency urgency)
{
    NotifyNotification* notification;

#ifdef NEW_LIBNOTIFY
    notification = notify_notification_new(summary, body, DATA_DIR "icons/48x48/xc-icon.png");
#else
    notification = notify_notification_new(summary, body, DATA_DIR "icons/48x48/xc-icon.png", NULL);
#endif
    notify_notification_set_timeout(notification, NOTIFY_TIMEOUT);
    notify_notification_set_urgency(notification, urgency);

    notify_notification_show(notification, NULL);
}

#if 0
static void notify_info(const char* summary, const char* body)
{
    notify(summary, body, NOTIFY_URGENCY_LOW);
}
#endif

static void notify_warning(const char* summary, const char* body)
{
    notify(summary, body, NOTIFY_URGENCY_NORMAL);
}

#if 0
static void notify_error(const char* summary, const char* body)
{
    notify(summary, body, NOTIFY_URGENCY_CRITICAL);
}
#endif

static DBusHandlerResult
cb_xenmgr(DBusConnection *connection, DBusMessage *message, void *user_data)
{
    const char *method;


    method = dbus_message_get_member(message);
    if (!method || !strcmp(method, "NameAcquired"))
        return DBUS_HANDLER_RESULT_HANDLED;

#ifdef DEBUG
    fprintf(stderr, "RECEIVED, method=%s\n",  method);
#endif

    if (method && !strcmp(method, "storage_space_low")) {
        char smsg[128] = {0};
        struct strbuf pcent = udbus_get_sval(message);
        if (pcent.len) {
            snprintf(smsg, 128, "Only %s%% of your disk space is currently free.", pcent.buf);
            notify_warning("Storage space low !", smsg);
        }
        strbuf_release(&pcent);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (method && !strcmp(method, "update_state_change")) {
        notify_warning("XenClient update is ready", "To apply this update, you must restart your physical computer.");
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (method && !strcmp(method, "device_rejected")) {
        struct strbuf *args = udbus_msg_strings(message, 2);
        if (args) {
            char smsg[256] = {0};
            snprintf(smsg, 256, "You have inserted a USB device that has been blocked by Administrator policy. Device: '%s'. Reason: '%s'", args[0].buf, args[1].buf);
            notify_warning("Device rejected", smsg);
            strbuf_release(args+1);
            strbuf_release(args);
            free(args);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    refresh_menu();
    return DBUS_HANDLER_RESULT_HANDLED;
}


static int udbus_init(void)
{
    app.err = (DBusError *)malloc(sizeof (DBusError));
    if (!app.err)
        goto out2;
    dbus_error_init(app.err);

    app.conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
    if (!app.conn)
        goto out1;

    dbus_bus_add_match(app.conn, "type='signal',interface='com.citrix.xenclient.xenmgr',path='/',member='vm_state_changed'", NULL);
    dbus_bus_add_match(app.conn, "type='signal',interface='com.citrix.xenclient.xenmgr',path='/',member='vm_created'", NULL);
    dbus_bus_add_match(app.conn, "type='signal',interface='com.citrix.xenclient.xenmgr',path='/',member='vm_deleted'", NULL);
    dbus_bus_add_match(app.conn, "type='signal',interface='com.citrix.xenclient.xenmgr.host',member='storage_space_low'", NULL);
    dbus_bus_add_match(app.conn, "type='signal',interface='com.citrix.xenclient.updatemgr',member='update_state_change'", NULL);
    dbus_bus_add_match(app.conn, "type='signal',interface='com.citrix.xenclient.usbdaemon',member='device_rejected'", NULL);
        dbus_connection_add_filter(app.conn, cb_xenmgr, app.loop, NULL);

    dbus_connection_setup_with_g_main(app.conn, NULL);

    return 0;

out1:
        dbus_error_free(app.err); 
    free(app.err);
    app.err = NULL;
out2:
    return -1;
}

#if 0
static void udbus_release()
{
    if (app.conn)
        dbus_connection_unref(app.conn);
    if (app.err)
        free(app.err);
    app.conn = NULL;
    app.err = NULL;
}
#endif


/* when called it MUST end with NULL in arguments list ! */
static DBusMessage * udbus_call(char *dest, char *path,
                                char *inf, char *method, ...)
{
    va_list params;
    char *p;
    DBusMessage *message, *reply = NULL;
    DBusMessageIter iter;
    if (!app.conn)
        return NULL;
    dbus_error_init(app.err);
    message = dbus_message_new_method_call(dest, path, inf, method);
    dbus_message_set_auto_start(message, TRUE);
    dbus_message_iter_init_append(message, &iter);

    va_start(params, method);
    while ((p = va_arg(params, char *))) {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &p);
    }
    va_end(params);

    reply = dbus_connection_send_with_reply_and_block(app.conn, message, -1, app.err); 
    if (dbus_error_is_set(app.err)) {
        fprintf(stderr, "error: %s: %s\n", app.err->name, app.err->message);
        goto out1;
    }
    if (reply)
        dbus_message_get_reply_serial(reply);

    dbus_message_unref(message);
    return reply;

out1:
    dbus_message_unref(message);
    if (reply)
        dbus_message_unref(reply);
    return NULL;
}

static struct strbuf** udbus_get_array(DBusMessage *reply)
{
    struct strbuf **ls = NULL;
    DBusMessageIter iter, subiter;
    int current_type;
    int idx = 0, lall = 2;
    if (!reply)
        return NULL;

    dbus_message_iter_init(reply, &iter);
    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
        return NULL;
    dbus_message_iter_recurse(&iter, &subiter);
    current_type = dbus_message_iter_get_arg_type (&subiter); 

    if (current_type == DBUS_TYPE_BYTE) {
        while (current_type != DBUS_TYPE_INVALID) {
            unsigned char val;
            if (!ls) {
                ls = xmalloc(sizeof (*ls));
                *ls = xmalloc (sizeof (**ls));
                strbuf_init(*ls, 0);
            }

            dbus_message_iter_get_basic(&subiter, &val);
            strbuf_add(*ls, &val, 1);

            dbus_message_iter_next(&subiter);
            current_type = dbus_message_iter_get_arg_type(&subiter);
        }
    } else {
        while (current_type != DBUS_TYPE_INVALID) {
            char *val;
            struct strbuf *sb;
            if (!ls) {
                ls = xcalloc(lall, sizeof(struct strbuf*));
            }
            if (idx + 1 >= lall) {
                lall = lall*2;
                ls = xrealloc(ls, lall * sizeof(struct strbuf *));
            }
            sb = xmalloc(sizeof(struct strbuf));
            strbuf_init(sb, 0);
            ls[idx] = sb;
            ls[++idx] = NULL;

            dbus_message_iter_get_basic(&subiter, &val);
            strbuf_addstr(sb, val);

            dbus_message_iter_next(&subiter);
            current_type = dbus_message_iter_get_arg_type(&subiter);
        }
    }

    dbus_message_unref(reply);
    return ls;
}

struct strbuf udbus_get_sval(DBusMessage *msg)
{
    DBusMessageIter miter, siter, *iter;
    char *val;
    dbus_uint32_t ival;
    struct strbuf sb = STRBUF_INIT;
    if (!msg)
        return sb;
    iter = &miter;
    dbus_message_iter_init(msg, iter);
    if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_VARIANT) {
        dbus_message_iter_recurse(iter, &siter);
        iter = &siter;
    }
    if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_STRING) {
        dbus_message_iter_get_basic(iter, &val);
        strbuf_addstr(&sb, val);
    } else if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_UINT32) {
        dbus_message_iter_get_basic(iter, &ival);
        strbuf_addf(&sb, "%u", ival);
    }
    return sb;
}

struct strbuf *udbus_msg_strings(DBusMessage *message, int number)
{
    DBusMessageIter miter, siter, *iter;
    char *val;
    struct strbuf *sb = NULL;
    int i;

    sb = (struct strbuf*) xmalloc(number * sizeof(struct strbuf));
    if (!sb)
        goto out_message;
    for (i = 0; i < number; i++)
        strbuf_init(sb+i, 0);

    iter = &miter;
    dbus_message_iter_init(message, iter);
    if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_VARIANT) {
        dbus_message_iter_recurse(iter, &siter);
        iter = &siter;
    }
    for (i = 0; i < number; i++) {
        if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_STRING) {
            break;
        }
        dbus_message_iter_get_basic(iter, &val);
        strbuf_addstr(sb+i, val);
        if (!dbus_message_iter_next(iter))
            break;
    }
out_message:
    if (message)
        dbus_message_unref(message);
    return sb;
}

static struct strbuf *udbus_get_strings(char *dest, char *path,
                                        char *inf, char *method,
                                        int number)
{
    DBusMessage *message = udbus_call(dest, path, "org.freedesktop.DBus.Properties", "Get", inf, method, NULL);
    if (!message)
        return NULL;
    return udbus_msg_strings(message, number);
}
dbus_uint32_t udbus_get_uint32(char *dest, char *path, char *inf, char *method)
{
    DBusMessageIter miter, siter, *iter;
    dbus_uint32_t val;

    DBusMessage *message = udbus_call(dest, path, "org.freedesktop.DBus.Properties", "Get", inf, method, NULL);
    if (!message)
        return (dbus_uint32_t)-1;

    iter = &miter;
    dbus_message_iter_init(message, iter);
    if (dbus_message_iter_get_arg_type(iter) == DBUS_TYPE_VARIANT) {
        dbus_message_iter_recurse(iter, &siter);
        iter = &siter;
    } else if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_UINT32) {
        return (dbus_uint32_t)-1;
    }
    dbus_message_iter_get_basic(iter, &val);
    if (message)
        dbus_message_unref(message);
    return val;
}

static void ui_vm_switch(GtkMenuItem* sender, gpointer sb)
{
    struct strbuf *path = (struct strbuf*) sb;
    DBusMessage *message;
    if (!path)
        return;
    
    message = udbus_call(XENMGR, path->buf, "com.citrix.xenclient.xenmgr.vm", "switch", NULL);
    if (message)
        dbus_message_unref(message);
}

#if 0
static void ui_vm_quit(GtkMenuItem* sender, gpointer unused)
{
    if (app.doms) {
        strbuf_list_free(app.doms);
        app.doms = NULL;
    }
    if (app.conn)
        udbus_release();
    g_main_loop_quit(app.loop);  
}
#endif

static GtkWidget* create_menu_item(char *name, int getIcon, gpointer sb)
{
    struct strbuf *path = (struct strbuf*) sb;
    GtkWidget *menu_item;
    DBusMessage *msg;
    GtkImage *img;

    menu_item = gtk_image_menu_item_new_with_label(name);

    // get icon
    if (getIcon)
    {
        struct strbuf **picon;
        msg = udbus_call(XENMGR, path->buf, "com.citrix.xenclient.xenmgr.vm", "read_icon", NULL);
        if (msg) {
            GInputStream *bstream;
            GdkPixbuf *bpix;
#ifndef LIBAPPINDICATOR
	    GdkPixbuf *bpix_scaled;
#endif
            picon = udbus_get_array(msg); // also unreferences msg
            if (picon && *picon) {
                bstream = g_memory_input_stream_new_from_data((*picon)->buf, (*picon)->len, NULL);
                if (bstream) {
                    bpix = gdk_pixbuf_new_from_stream(bstream, NULL, NULL);
                    if (bpix) {
#ifdef LIBAPPINDICATOR
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), gtk_image_new_from_pixbuf(bpix));
			gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
#else
			bpix_scaled = gdk_pixbuf_scale_simple(bpix, 24, 24, GDK_INTERP_BILINEAR);
			img = gtk_image_new_from_pixbuf(bpix_scaled);
			gtk_image_set_pixel_size(img, 48);
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), img);
			gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(menu_item), TRUE);
			g_object_unref(bpix_scaled);
#endif
                        g_object_unref(bpix);
                    }
                    g_input_stream_close(bstream, NULL, NULL);
                    g_object_unref(bstream);
                }
                strbuf_free(*picon);
                free(picon);
            }
        }
    }

    g_signal_connect(GTK_OBJECT(menu_item), "activate", (GCallback) ui_vm_switch, path);

    return menu_item;
}

int refresh_menu(void)
{
    GtkWidget *menu_item, *home_item;
    DBusMessage *msg;
    struct strbuf  **idom, *sb;

    home_item = NULL;

    if (!app.ind)
        return -1;

    if (app.menu) {
        /* clean the menu */
        if (app.doms) {
            strbuf_list_free(app.doms);
            app.doms = NULL;
        }

        gtk_widget_destroy((GtkWidget*)app.menu);
        app.menu = NULL;
    }

    app.menu = gtk_menu_new();

    msg = udbus_call(XENMGR, "/", XENMGR, "list_vms", NULL);
    if (!msg)
        return -1;

    app.doms = udbus_get_array(msg);
    if (!app.doms)
        return -1;

    // we should sort this by slot!!

    idom = app.doms;
    while ((sb = *(idom++))) {
        struct strbuf *name, *vm_state, *type;
        unsigned int slot;
        int crt_vm, hidden;

        name = udbus_get_strings(XENMGR, sb->buf, "com.citrix.xenclient.xenmgr.vm", "name", 1);
        vm_state = udbus_get_strings(XENMGR, sb->buf, "com.citrix.xenclient.xenmgr.vm", "state", 1);
        slot = udbus_get_uint32(XENMGR, sb->buf, "com.citrix.xenclient.xenmgr.vm", "slot");
        hidden = udbus_get_uint32(XENMGR, sb->buf, "com.citrix.xenclient.xenmgr.vm", "hidden-in-ui");
        type = udbus_get_strings(XENMGR, sb->buf, "com.citrix.xenclient.xenmgr.vm", "type", 1);

#if DEBUG
        fprintf(stderr, "%d - %s (%s)\n", slot, name->buf, sb->buf);
#endif

        // Only show running VMs
        if (strcmp(vm_state->buf, "running"))
            goto cont;

        // UIVM
        if (slot == 0)
        {
            home_item = create_menu_item("XenClient™ XT", FALSE, sb);
            goto cont;          
        }

        // Only show VMs in slot range and not hidden
        if (slot > 9 || hidden)
            goto cont;

        // Only show user VMs (not PVM and not SVM)
        if (strcmp(type->buf, "pvm") && strcmp(type->buf, "svm"))
            goto cont;

        crt_vm = (app.vm_path.len && !strcmp(sb->buf, app.vm_path.buf));

        menu_item = create_menu_item(name->buf, TRUE, sb);

        if (crt_vm)
            gtk_widget_set_sensitive(menu_item, FALSE);

        gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), menu_item);

cont:
        strbuf_free(name);
        strbuf_free(vm_state);
        strbuf_free(type);
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), gtk_separator_menu_item_new());

//  home_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
//  g_signal_connect(GTK_OBJECT(home_item), "activate", (GCallback) ui_vm_quit, 0);

    gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), home_item);

#ifdef LIBAPPINDICATOR
    app_indicator_set_menu (app.ind, GTK_MENU (app.menu));
#endif

    gtk_widget_show_all((GtkWidget*)app.menu);

    return 0;
}

#ifndef LIBAPPINDICATOR

static void show_menu()
{
    gtk_menu_popup (app.menu, NULL, NULL, gtk_status_icon_position_menu, app.ind, NULL, gtk_get_current_event_time());
}

static void tray_icon_on_click(GtkStatusIcon *status_icon,
			       gpointer user_data)
{
    // Clicked on tray icon
    show_menu();
}

static void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button,
			      guint activate_time, gpointer user_data)
{
    // Popup menu
    show_menu();
}


static GtkStatusIcon *create_tray_icon() {

    GtkStatusIcon *tray_icon;

    tray_icon = gtk_status_icon_new_from_file("/usr/share/xc-switcher/icons/24x24/xc-icon.png");
    g_signal_connect(G_OBJECT(tray_icon), "activate",
		     G_CALLBACK(tray_icon_on_click), NULL);
    g_signal_connect(G_OBJECT(tray_icon),
		     "popup-menu",
		     G_CALLBACK(tray_icon_on_menu), NULL);

    gtk_status_icon_set_tooltip(tray_icon,
				"XenClient XT Switcher Bar");
    gtk_status_icon_set_visible(tray_icon, TRUE);

    return tray_icon;
}

#endif

int main(int argc, char **argv)
{
#ifdef LIBAPPINDICATOR
    AppIndicator *ind;
#else
    GtkStatusIcon *ind;
#endif

    memset(&app, 0, sizeof(struct app_state));
    strbuf_init(&app.vm_path, 0);
    if (argc > 1) {
        char *c;
        strbuf_addstr(&app.vm_path, argv[1]);
        c = app.vm_path.buf;
        while (*c++)
            if (*c == '-')
                *c = '_';
    }

    g_type_init();
    gtk_init(&argc, &argv);

    app.loop = g_main_loop_new(NULL, FALSE);

#ifdef LIBAPPINDICATOR
    ind = app_indicator_new("indicator-domains", "domain", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    if (!ind)
        goto fail1;

    app_indicator_set_status(ind, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_attention_icon(ind, "indicator-messages-new");
    app_indicator_set_icon_full(ind, DATA_DIR "icons/48x48/xc-icon.png", NULL);
#else
    ind = create_tray_icon();
#endif

    notify_init(APP_NAME);
    app.ind = ind;

    udbus_init();
    refresh_menu();

    g_main_loop_run(app.loop);

    return 0;
fail1:
    return 1;
}
