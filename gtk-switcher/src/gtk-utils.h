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

#ifndef GTK_UTILS_H_
#define GTK_UTILS_H_

#include <gtk/gtk.h>

gint get_screen_width();
GdkPixbuf* buffer_from_file(const char* name);
GtkImage* image_from_file(const char* name);
GtkEventBox* image_box_from_image(GtkImage* image);
GtkEventBox* image_box_from_file(const char* name);
void tile_background(GtkWidget* window, const char* name);

#endif
