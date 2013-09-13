/* vim: set sw=4 ts=4 et : */
/* log.h: logging functions
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
#ifndef _ARMOUR_LOG_H
#define _ARMOUR_LOG_H

#include <syslog.h>

extern int _verbose;

#define log_error(format, ...)                          \
    log_printf (LOG_ERR, format, ##__VA_ARGS__)

#define log_info(format, ...)                           \
    log_printf (LOG_INFO, format, ##__VA_ARGS__)

#define log_debug(format, ...)			                \
    do {                                                \
        if (_verbose) log_printf (LOG_DEBUG,            \
        "%s:%u:%s: " format,                            \
        __FILE__, __LINE__, __func__, ##__VA_ARGS__);   \
    } while (0)

void log_printf (int level, const char *format, ...)
	__attribute__ ((format (printf, 2, 3)));
void enable_syslog (const char *name);

#endif /* ARMOUR_LOG_H */
