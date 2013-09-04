/* vim: set sw=4 ts=4 et : */
/* armour_watchdog.h: watchdog interface
 *
 * Copyright (C) 2013 Phil Chen
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
#ifndef _ARMOUR_WATCHDOG_H
#define _ARMOUR_WATCHDOG_H

#define WATCHDOG_DEV		"/dev/watchdog"
#define WATCHDOG_TIMEOUT	30
#define WATCHDOG_KEEPALIVE	15

typedef struct {
	int fd;
	int options;
	int timeout;
	int pretimeout;
	int keepalive;
} armour_watchdog;

int armour_watchdog_init(armour_watchdog *);
void armour_watchdog_ping(armour_watchdog *);

#endif //_ARMOUR_WATCHDOG_H
