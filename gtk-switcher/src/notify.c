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

#include <libnotify/notify.h>

#define APP_NAME "xenclient_gtk_switcher"
#define NOTIFY_TIMEOUT 60000

int
notify_setup()
{
	return notify_init(APP_NAME);
}

void
notify(const char* summary, const char* body, NotifyUrgency urgency)
{
    NotifyNotification* notification;
    
    notification = notify_notification_new(summary, body, NULL, NULL);
    notify_notification_set_timeout(notification, NOTIFY_TIMEOUT);
    notify_notification_set_urgency(notification, urgency);

    notify_notification_show(notification, NULL);
}

void
notify_info(const char* summary, const char* body)
{
    notify(summary, body, NOTIFY_URGENCY_LOW);
}

void
notify_warning(const char* summary, const char* body)
{
    notify(summary, body, NOTIFY_URGENCY_NORMAL);
}

void
notify_error(const char* summary, const char* body)
{
    notify(summary, body, NOTIFY_URGENCY_CRITICAL);
}
