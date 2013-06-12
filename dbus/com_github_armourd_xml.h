/* vim: set sw=4 ts=4 et : */
/* armour_dbus_interface.c: dbus constants
 *
 * Copyright (C) 2012 Alexandre Moreno
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#ifndef _ARMOUR_DBUS_INTROSPECT_H
#define _ARMOUR_DBUS_INTROSPECT_H

#define COM_GITHUB_ARMOURD_XML \
"<node>\n"\
"<interface name=\"org.freedesktop.DBus.Introspectable\">\n"\
"<method name=\"Introspect\">\n"\
"<arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n"\
"</method>\n"\
"</interface>\n"\
"<interface name=\"com.github.Armourd\">\n"\
"<method name=\"ListProcesses\">\n"\
"<arg type=\"a{sv}\" direction=\"out\"/>\n"\
"</method>\n"\
"<method name=\"WatchProcess\">\n"\
"<arg name=\"pid\" type=\"i\" direction=\"in\"/>\n"\
"<arg name=\"resultcode\" type=\"u\" direction=\"out\"/>\n"\
"</method>\n"\
"</interface>\n"\
"</node>\n"\

enum {
    ARMOUR_DBUS_REPLY_SUCCESS = 0,
    ARMOUR_DBUS_REPLY_ALREADY_WATCHED = 1,
    ARMOUR_DBUS_REPLY_NOMEM = 2,
};

#endif
