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
#include <inttypes.h>

#include "armour_proc.h"

typedef struct armour armour_t;
typedef struct armour_event armour_event;

struct armour_event
{
    int (*cb)(int , uint32_t, void *);  /**< function pointer to event handler */
    int fd;                             /**< fd/sock associated with the event */
    void *data;                         /**< user data */
    armour_event *next;                 /**< next event handler */
};

struct armour 
{
    int epollfd;                /**< main loop */
    int running;                /**< break main loop */
    armour_event signal, sock;  /**< handlers for signals and netlink datagrams */
    armour_event *events;       /**< list of other event handlers */
    armour_proc *head;          /**< list of processes that are watched */
};

int             armour_init             (armour_t **handle, const char *config_file);
int             armour_config_read      (armour_t *self, const char *file);
int             armour_run              (armour_t *self);
int             armour_add_signalfd     (armour_t *self);
int             armour_add_nlsock       (armour_t *self);
void            armour_apply_foreach    (armour_t *self, armour_proc_func *func, void *data);
armour_proc    *armour_lookup           (armour_t *self, armour_proc_func *func, void *data);
int             armour_listen           (armour_t *self);
int             armour_add_filter       (armour_t *self);
int             armour_nlsock_handler   (int fd, uint32_t events, void *data);
int             armour_signal_handler   (int fd, uint32_t events, void *data);
int             armour_remove_watch     (armour_t *self, pid_t pid);
int             armour_recover          (armour_t *self, pid_t pid);
int             armour_update_watch     (armour_t *self, pid_t pid);
int             armour_add_watch        (armour_t *self, pid_t pid);

#define LIST_ADD(head, item)    \
    do {                        \
        item->next = head;      \
        head = item;            \
    } while (0)

#define LIST_REM(head, item)    \
({                              \
    typeof (head) *_pp = &head; \
    typeof (item) _p = *_pp;    \
    while (_p != NULL) {        \
        if (_p == item) {       \
            *_pp = _p->next;    \
            _p->next = NULL;    \
            break;              \
        }                       \
        _pp = &_p->next;        \
        _p = _p->next;          \
    }                           \
    _p;                         \
})

static inline void armour_add_event (armour_t *self, armour_event *e)
{
    LIST_ADD (self->events, e);
}

static inline armour_event *armour_rem_event (armour_t *self, armour_event *e)
{
    return LIST_REM (self->events, e);
}

static inline void armour_add_proc (armour_t *self, armour_proc *p)
{
    LIST_ADD (self->head, p);
}

static inline armour_proc *armour_rem_proc (armour_t *self, armour_proc *p)
{
    return LIST_REM (self->head, p);
}

#endif /* _ARMOUR_H */
