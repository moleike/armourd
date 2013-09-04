/* vim: set sw=4 ts=4 et : */
/* armour_dbus.h: dbus interface
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
#ifndef _ARMOUR_DBUS_H
#define _ARMOUR_DBUS_H

#include "armour.h"
#include "config.h"

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
int armour_dbus_init (armour_t *self);
#else
static inline 
int armour_dbus_init (armour_t *self) { return 0; }
#endif

#endif /* _ARMOUR_DBUS_H */
