/* vim: set sw=4 ts=4 et : */
/* armour_watchdog.c: watchdog interface
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include "armour_watchdog.h"

void armour_watchdog_ping(armour_watchdog *wd)
{
	ioctl (wd->fd, WDIOC_KEEPALIVE, 0);
}

int armour_watchdog_init(armour_watchdog *wd)
{
	struct watchdog_info ident;
	char stop = 'V';

	wd->fd = open (WATCHDOG_DEV, O_WRONLY);
	if (wd->fd < 0)
		return -1;

	if (ioctl (wd->fd, WDIOC_GETSUPPORT, &ident) == -1)
		goto watchdog_init_fail;

	wd->options = ident.options;

	if (wd->options & WDIOF_SETTIMEOUT) {
		int timeout;

		wd->timeout = WATCHDOG_TIMEOUT;
		wd->keepalive = WATCHDOG_KEEPALIVE;
		if (ioctl (wd->fd, WDIOC_SETTIMEOUT, &wd->timeout) == -1)
			goto watchdog_init_fail;
		
		ioctl (wd->fd, WDIOC_GETTIMEOUT, &timeout);
	} else {
		if (ioctl (wd->fd, WDIOC_GETTIMEOUT, &wd->timeout) == -1)
			goto watchdog_init_fail;
		
		wd->keepalive = wd->timeout / 2;
	}

	if (wd->options & WDIOF_PRETIMEOUT) {
		wd->pretimeout =  wd->timeout / 3;
		if (ioctl (wd->fd, WDIOC_SETPRETIMEOUT, &wd->pretimeout) == -1)
			goto watchdog_init_fail;
	}

	armour_watchdog_ping (wd);

	return 0;

watchdog_init_fail:
	write (wd->fd, &stop, 1);
	close (wd->fd);
	wd->fd = 0;
	return -1;
}

