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

#include <gtk/gtk.h>

gint
get_monitor_width(GdkScreen* screen)
{
    gint monitors;
    gint width;
    
    monitors = gdk_screen_get_n_monitors(screen);
    
    if (monitors < 2) {
		width = gdk_screen_get_width(screen);
    } else {
		GdkRectangle rect;
		gdk_screen_get_monitor_geometry (screen, 0, &rect);
		width = rect.width;
    }
    
    return width;
}

GdkPixbuf*
buffer_from_file(const char* name)
{
    // TODO: cache loaded images
    gchar* path;
    GdkPixbuf* buffer;
     
    path = g_build_filename(PIXMAPS_DIR, name, NULL);
    buffer = gdk_pixbuf_new_from_file(path, NULL);
     
    g_free(path);
    return buffer;
}

GtkImage*
image_from_file(const char* name)
{    
    GdkPixbuf* buffer;
    GtkImage* image;

    buffer = buffer_from_file(name);
    image = (GtkImage*) gtk_image_new_from_pixbuf(buffer);

    return image;
}

GtkEventBox*
image_box_from_image(GtkImage* image)
{
    GtkEventBox* box;
    
    box = (GtkEventBox*) gtk_event_box_new();
    gtk_event_box_set_visible_window(box, FALSE);
    gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(image));
    
    return box;
}

GtkEventBox*
image_box_from_file(const char* name)
{
    GtkImage* image;
    GtkEventBox* box;
    
    image = image_from_file(name);
    box = image_box_from_image(image);
    gtk_widget_set_events((GtkWidget*) box, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    
    return box;
}

void
tile_background(GtkWidget* window, const char* name)
{
	GdkPixbuf* buffer;
	GdkPixmap* background;
	GtkStyle* style;
	
	buffer = buffer_from_file(name);
	gdk_pixbuf_render_pixmap_and_mask (buffer, &background, NULL, 0);
	style = gtk_style_new ();
	style->bg_pixmap[0] = background; 
	gtk_widget_set_style(window, style);
}
