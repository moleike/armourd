/* vim: set sw=4 ts=4 et : */
/* log.c: logging functions
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
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include "log.h"

static int use_syslog = 0;
int _verbose = 0;

void log_printf (int level, const char *fmt, ...)
{
	va_list va;
	va_start (va, fmt);
	if (use_syslog)
		vsyslog (level, fmt, va);
	else
		vfprintf (stderr, fmt, va);
	va_end (va);
}

void enable_syslog (const char *name)
{
	openlog (name, LOG_PID, LOG_DAEMON);
	use_syslog = 1;
}
