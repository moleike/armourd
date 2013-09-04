/* vim: set sw=4 ts=4 et : */
/* armour.h: process tracking
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
#ifndef _ARMOUR_EVENTS_H
#define _ARMOUR_EVENTS_H

#include <inttypes.h>

typedef struct armour_evdata armour_evdata;
typedef struct armour_evnode armour_evnode;

struct armour_evdata
{
    int (*cb)(int , uint32_t, void *);  /**< function pointer to event handler */
    int fd;                             /**< fd/sock associated with the event */
    void *user_data;                    /**< user data */
};

struct armour_evnode
{
    struct armour_evdata;
    armour_evnode *next;
};

#endif
