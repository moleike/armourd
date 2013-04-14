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
#include "armour_proc.h"

struct armour {
    int running;
    int epollfd;
    int sock;
    int signalfd;
    armour_proc *head;
};

typedef         struct armour           armour_t;
int             armour_init             (armour_t **handle, const char *config_file);
int             armour_config_read      (armour_t *self, const char *file);
int             armour_run              (armour_t *self);
void            armour_add              (armour_t *self, armour_proc *item);
void            armour_rem              (armour_t *self, armour_proc *item);
void            armour_apply_foreach    (armour_t *self, armour_proc_func *func, void *data);
armour_proc    *armour_lookup           (armour_t *self, armour_proc_func *func, void *data);
int             armour_listen           (armour_t *self);
int             armour_filter           (armour_t *self);
int             armour_handle           (armour_t *self);
int             armour_remove_watch     (armour_t *self, pid_t pid);
int             armour_recover          (armour_t *self, pid_t pid);
int             armour_update_watch     (armour_t *self, pid_t pid);
int             armour_add_watch        (armour_t *self, pid_t pid);

#endif /* _ARMOUR_H */
