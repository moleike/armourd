/* vim: set sw=4 ts=4 et : */
/* debug.h
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
#ifndef _DEBUG_H
#define _DEBUG_H

#include <err.h>

#ifndef DEBUG
#   define __DEBUG 0
#else   
#   define __DEBUG 1
#endif
/*
 * we do want the debug statements to be seen by the compiler,
 * they will be optimized away when not in a debug build
 */
#define DPRINT(fmt,...)    \
            do { if (__DEBUG) warnx(fmt, ##__VA_ARGS__); } while (0)
#define DWARN(fmt,...)     \
            do { if (__DEBUG) warn(fmt, ##__VA_ARGS__); } while (0)

#endif // _DEBUG_H
