/*
 * Copyright (c) 2012 Citrix Systems, Inc.
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

#define _USE_MATH_DEFINES
#include <math.h>
#include <gtk/gtk.h>
#include <dbus/dbus-glib.h>
#include "project.h"
#include "rpc.h"

static GtkWidget* window = NULL;
static GMainLoop* loop = NULL;
static GtkWidget* mainbox = NULL;
static GtkImage* handle = NULL;
static GtkWidget* home = NULL;
static GtkWidget* vmContainer = NULL;
static GPtrArray* vms = NULL;
static int barOpen = TRUE;
static int barAnimating = FALSE;
static struct timespec timer_start;
static gulong homeHandler = 0;

/* Bar Animations */

gboolean
timeout_cb () {
    
    int end = barOpen ? -77 : 0;
    int start = barOpen ? 0 : -77;
    double anim = 200*1000000; //nanos in a milli
    struct timespec timer_now;
    
    clock_gettime(CLOCK_MONOTONIC, &timer_now);
    double nano = ((timer_now.tv_sec - timer_start.tv_sec)* 1000000000) + (timer_now.tv_nsec - timer_start.tv_nsec);
    double state = nano / anim;
    double pos = ((-cos(state * M_PI) / 2) + 0.5);
    int Y = (int)(start + ((end - start) * pos));
    gtk_window_move((GtkWindow*) window, 0, Y);
             
    if (nano >= anim) // end condition
    {
        GdkPixbuf* buffer;
                        
        buffer = (GdkPixbuf*) buffer_from_file(barOpen ? "handle_closed.png" : "handle_open.png");   
        gtk_image_set_from_pixbuf(handle, buffer);
        barOpen = !barOpen;
        barAnimating = FALSE;
    
        return FALSE;
    }
    
    return TRUE;
}

gboolean
update_bar()
{
    if (!barAnimating) {
        barAnimating = TRUE;
        clock_gettime(CLOCK_MONOTONIC, &timer_start);
        g_timeout_add(100, timeout_cb, NULL);
    }
    
    return FALSE;
}

/* Button Creation */

static gboolean
button_press_callback (GtkWidget* event_box, GdkEventButton *event, const char *vm)
{
    printf("switch to %s\n", vm);
    vm_switch( vm );
    update_bar();
}

GtkEventBox*
create_vm_box(const char* vm)
{
	GtkEventBox* box;
	GtkFixed* fixed;
	GtkLabel* label;
	
	char markup[1024] = {0};
	const char* name;
	
	GdkPixbuf* bevel;
	GdkPixbuf* icon;
	GtkImage* image;
	GArray *arr;
	
	box = (GtkEventBox*) gtk_event_box_new();
   
	gtk_event_box_set_visible_window(box, FALSE);
	fixed = (GtkFixed*) gtk_fixed_new();
	label = (GtkLabel*) gtk_label_new(NULL);
	name = vm_get_name(vm);

	if (vm_is_guest(vm))
	{
		sprintf(markup, "<span weight='bold' font='10' color='white'>%s</span>", name);		
		bevel = (GdkPixbuf*) buffer_from_file("bevel.png");
	}
	else
	{
		sprintf(markup, "<span font='9' color='white'>%s</span>", name);
		bevel = (GdkPixbuf*) buffer_from_file("no_bevel.png");
	}

	arr = vm_get_icon_bytes( vm );
	if (arr) {
		icon = (GdkPixbuf*) get_icon( arr );
		if (icon) {
			gdk_pixbuf_composite(icon, bevel, 0, 0, 105, 64, 27, 0, 0.2, 0.2, GDK_INTERP_BILINEAR, 255);
		}
		g_array_free( arr, TRUE );
	}

    image = (GtkImage*) gtk_image_new_from_pixbuf(bevel);

	gtk_label_set_markup(label, markup);
	gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_size_request(GTK_WIDGET(label), 95, 20);

	gtk_fixed_put(fixed, GTK_WIDGET(image), 0, 0);
	gtk_fixed_put(fixed, GTK_WIDGET(label), 5, 44);

    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(fixed));
	gtk_widget_set_tooltip_text(GTK_WIDGET(box), name);

	gtk_widget_show_all(GTK_WIDGET(box));

    g_free((char*)name);
    
    return box;
}

static gint
vm_sorter(const gchar** vm_a, const gchar** vm_b)
{
	return vm_get_slot(*vm_a) > vm_get_slot(*vm_b);
}

static void
update_vms( void )
{
    int i;
    if (vms) {
        /* clean */
        g_ptr_array_free( vms, TRUE );
    }
    vms = list_vms();
	g_ptr_array_sort(vms, (GCompareFunc) vm_sorter);
   
    if (vmContainer) {
        gtk_container_remove( GTK_CONTAINER(mainbox), vmContainer );
        gtk_widget_destroy((GtkWidget*) vmContainer);
        vmContainer = NULL;
    }
    
    if (vms) {
        /* debug */
        dump_vms(vms);

        vmContainer = gtk_hbox_new ( FALSE, 0);

        gtk_container_add (GTK_CONTAINER (mainbox), vmContainer );
        gtk_widget_show(vmContainer);

        for (i = 0; i < vms->len; ++i ) {
            
            const char *vm = g_ptr_array_index( vms, i );
                       
            int slot = vm_get_slot(vm);
            GtkEventBox* box;
            
            if (slot == 0 && !homeHandler) {
				box = (GtkEventBox*) home;
				homeHandler = g_signal_connect(G_OBJECT(box), "button_press_event", G_CALLBACK(button_press_callback), (void*)vm);
			} else if (vm_is_visible(vm)) {
                box = create_vm_box(vm);
                gtk_box_pack_start((GtkBox*)vmContainer, GTK_WIDGET(box), FALSE, FALSE, 5);
				g_signal_connect(G_OBJECT(box), "button_press_event", G_CALLBACK(button_press_callback), (void*)vm);
            }
        }
    }
}

/* Switcher Shape */
 
static GdkDrawable*
create_mask(gint width, gint height)
{
    //           Layout 'Jockstrap'
    //   *------------------------------*
    //   |                              |
    //   *--------*            *--------*
    //            |            |        
    //            *            *
    //             \          /
    //              *--------*    

    GdkDrawable* mask;
    cairo_t* context;
    
    gint handle_height = 11;
    gint handle_width = 118;
    gint handle_chop = 4;

    mask = (GdkDrawable*) gdk_pixmap_new (NULL, width, height, 1);
    context = gdk_cairo_create(mask);

    // Clear mask
    cairo_save (context);
    cairo_rectangle (context, 0, 0, width, height);
    cairo_set_operator (context, CAIRO_OPERATOR_CLEAR);
    cairo_fill (context);
    cairo_restore (context);

    // Draw mask
    cairo_move_to (context, 0, 0);
    cairo_line_to (context, 0, height - handle_height);

    cairo_line_to (context, (width - handle_width)/2, height - handle_height);
    cairo_line_to (context, (width - handle_width)/2, height - handle_chop);
    cairo_line_to (context, (width - handle_width)/2 + handle_chop, height);

    cairo_line_to (context, (width + handle_width)/2 - handle_chop, height);
    cairo_line_to (context, (width + handle_width)/2, height - handle_chop);
    cairo_line_to (context, (width + handle_width)/2, height - handle_height);

    cairo_line_to (context, width, height - handle_height);
    cairo_line_to (context, width, 0);

    cairo_close_path (context);
      
    // Fill mask
    cairo_set_source_rgb (context, 1, 1, 1);
    cairo_fill (context);

    cairo_destroy (context);
      
    return mask;
}

/* Event handlers */

static void
destroy(void)
{
    g_main_loop_quit(loop);
}

static gboolean
hover_cb (GtkWidget *event_box, GdkEventCrossing *event, gpointer* data)
{    
    if ((event->detail == GDK_NOTIFY_NONLINEAR_VIRTUAL || event->detail == GDK_NOTIFY_NONLINEAR) && barOpen) {
        update_bar();
    }
}

static gboolean
handle_cb (GtkWidget *event_box, GdkEventButton *event, gpointer* data)
{
	update_bar();
}

static void
handle_vm_changed (DBusGProxy* proxy, char *uuid, char *vm, gpointer udata)
{
    update_vms();
}

static void
handle_vm_state_changed (DBusGProxy* proxy, char *uuid, char *vm, char* state, int acpi_state, gpointer udata)
{
    update_vms();
}

static void
handle_storage_space_low (DBusGProxy* proxy, int percent, gpointer udata)
{
	char message[1024] = {0};
	
	sprintf(message, "Only %d%% of your disk space is currently free.", percent);
    notify_error("Disk space low", message);
}

static void
handle_update_state_change (DBusGProxy* proxy, char* state, gpointer udata)
{
	if (!strcmp(state, "downloaded-files"))
	{
		notify_warning("XenClient update is ready", "To apply this update, you must restart your physical computer.");
	}
}

/* Main */

int
main(int argc, char *argv[])
{
    GdkScreen* screen;
    GdkDrawable* mask;
    GtkWidget* table;
    GtkImage* blanker;
    GtkWidget* handle_box;
    
    gint width;
    gint height = 75 + 11;
    int result = 0;

    openlog("gtk-switcher", 0, LOG_USER);
    info("starting");

    gtk_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);
  
    result = notify_setup();
    if (result < 0)
    {
        return result;
    }    

    info("initialised notify");
    
	result = xenstore_init();
    if (result < 0)
    {
        return result;
    }    

    info("initialised xenstore");
    
    result = rpc_init(loop);
    if (result < 0)
    {
        return result;
    }
    
    info("initialised rpc");

    screen = gdk_screen_get_default();
    width = get_monitor_width(screen);
    mask = create_mask(width, height);
    window = gtk_window_new (GTK_WINDOW_POPUP);
  
    gtk_window_set_default_size(GTK_WINDOW (window), width, height);
    gtk_window_set_decorated(GTK_WINDOW (window), FALSE);
    gtk_window_set_opacity(GTK_WINDOW (window), 0.9);  
    gtk_widget_shape_combine_mask((GtkWidget*) window, mask, 0, 0);
    
    tile_background(window, "background.png");

    gtk_signal_connect(GTK_OBJECT (window), "destroy", GTK_SIGNAL_FUNC (destroy), NULL);
    
    info("initialised window");

    home = (GtkWidget*) image_box_from_file("xenclient.png");
    handle = (GtkImage*) image_from_file("handle_open.png");
    blanker = (GtkImage*) image_from_file("blanker.png");
 
    info("initialised graphics");

    table = gtk_table_new(2, 3, FALSE);
    mainbox = gtk_hbox_new (FALSE, 0);
    handle_box = (GtkWidget*) image_box_from_image(handle);
    
    info("initialised widgets");
    
    gtk_table_attach(GTK_TABLE(table), home, 0, 1, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(table), (GtkWidget*) blanker, 2, 3, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(table), handle_box, 1, 2, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(table), mainbox, 1, 2, 0, 1, GTK_EXPAND, GTK_EXPAND, 0, 0);
    gtk_container_add(GTK_CONTAINER(window), table);

    gtk_widget_show_all(window);
    
    info("shown window");

    /* signal handlers */
    g_signal_connect (G_OBJECT (window), "leave-notify-event", G_CALLBACK(hover_cb), NULL);
    g_signal_connect (G_OBJECT (handle_box), "enter-notify-event", G_CALLBACK(handle_cb), NULL);
    
    on_vm_changed(handle_vm_changed, NULL);
    on_vm_state_changed(handle_vm_state_changed, NULL);
    on_storage_space_low(handle_storage_space_low, NULL);
	on_update_state_change(handle_update_state_change, NULL);

    g_timeout_add(1000, update_bar, NULL);

    /* initial vm query */
    update_vms();

    g_main_loop_run(loop);
}
