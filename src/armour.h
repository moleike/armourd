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
#ifndef _ARMOUR_H
#define _ARMOUR_H

#include <sys/types.h>
#include <stddef.h>

#include "armour_proc.h"
#include "events.h"
#include "watchdog.h"

struct armour 
{
    int epollfd;                /**< main loop */
    int running;                /**< break main loop */
	armour_watchdog watchdog;	/**< handler for watchdog interface */
	armour_evdata signal, sock; /**< handlers for signals and netlink datagrams */
    armour_proc *head;          /**< list of processes that are watched */
};

typedef         struct armour           armour_t;
int             armour_init             (armour_t **self, const char *config_file);
int             armour_run              (armour_t *self);
int             armour_config_read      (armour_t *self, const char *config_file);
armour_proc    *armour_add_new          (armour_t *self, const char *filepath);          
int             armour_add_watch        (armour_t *self, pid_t pid);
armour_proc    *armour_lookup_pid       (armour_t *self, pid_t pid);
armour_proc    *armour_lookup_exe       (armour_t *self, const char *path);
int             armour_remove_watch     (armour_t *self, pid_t pid);
int             armour_recover          (armour_t *self, pid_t pid);
int             armour_update_watch     (armour_t *self, pid_t pid);

#endif /* _ARMOUR_H */
