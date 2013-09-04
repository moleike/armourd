/* vim: set sw=4 ts=4 et : */
/* armour_dbus.c: dbus interface
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
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <time.h>
#include <inttypes.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include "armour.h"
#include "list.h"
#include "debug.h"
#include "events.h"
#include "com_github_armourd_xml.h"
#include "armour_dbus.h"

#define ARMOURD_DBUS_INTERFACE "com.github.Armourd"
#define ARMOURD_DBUS_SERVICE "com.github.Armourd"
#define armourd_dbus_introspect_xml             \
    DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE   \
    COM_GITHUB_ARMOURD_XML

void dispatch_status_changed (DBusConnection *connection, 
                              DBusDispatchStatus new_status, 
                              void *data)
{
    armour_evdata *event = data;
    uint64_t u = 1;

    DPRINT ("dispatch status changed to %d!", new_status);

    if (new_status == DBUS_DISPATCH_DATA_REMAINS)
        if (write (event->fd, &u, sizeof u) < 0) /* this could block! */
            warn ("write eventfd");
}

int armour_dbus_dispatch_handle (int fd, unsigned int event, void *data)
{
    DBusConnection *connection = data;
    uint64_t u;
    (void) event;

    DPRINT ("inside dispatch handle!");

    while (DBUS_DISPATCH_DATA_REMAINS == 
        dbus_connection_dispatch (connection));

    read (fd, &u, sizeof u);
    return 0;
}

int armour_dbus_timeout_handle (int fd, unsigned int event, void *data)
{
    DBusTimeout *timeout = data;
    uint64_t exp;

    if (read (fd, &exp, sizeof exp) < 0)
        return -1;

    dbus_timeout_handle (timeout);
    return 0;
}

dbus_bool_t add_timer (DBusTimeout *timeout, void *data)
{
    armour_t *self = data;
    armour_evdata *event;
    int fd, ret;
    struct epoll_event ev;
    
    event = malloc (sizeof *event);
    if (!event)
        return FALSE;
    
    fd = timerfd_create (CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (fd < 0)
        return FALSE;

    *event = (struct armour_evdata) { .cb = armour_dbus_timeout_handle, 
                                     .fd = fd, .user_data = timeout };

    ev = (struct epoll_event) { EPOLLIN, { .ptr = event } };

    ret = epoll_ctl (self->epollfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0) {
        close (fd);
        free (event);
        return FALSE;
    }

    dbus_timeout_set_data (timeout, event, free);

    return TRUE;
}

void remove_timer (DBusTimeout *timeout, void *data)
{
    armour_t *self = data;
    armour_evdata *event;
    
    event = dbus_timeout_get_data (timeout);
    if (!event)
        return;
    
    epoll_ctl (self->epollfd, EPOLL_CTL_DEL, event->fd, NULL);
    close (event->fd);
}

void toggle_timer (DBusTimeout *timeout, void *data)
{
    armour_evdata *event;
    (void) data;
    
    event = dbus_timeout_get_data (timeout);
    if (!event)
        return;

    if (TRUE == dbus_timeout_get_enabled (timeout)) {
        int interval = dbus_timeout_get_interval (timeout);
        struct timespec spec;
        spec.tv_sec = interval / 1000;
        spec.tv_nsec = interval % 1000 * 1000000;

        if (timerfd_settime (event->fd, 0, 
                &(struct itimerspec) { spec, spec }, NULL) < 0)
            return;
    } else {
        if (timerfd_settime (event->fd, 0, 
                &(struct itimerspec) { { 0 } }, NULL) < 0)
            return;
    }
}

int armour_dbus_watch_handle (int fd, unsigned int event, void *data)
{
    DBusWatch *watch = data;
    DBusConnection *connection = dbus_bus_get (DBUS_BUS_SESSION, NULL);
    unsigned int flags = 0;

    if (event & EPOLLIN) {
        flags |=  DBUS_WATCH_READABLE;
    }
    if (event & EPOLLOUT) {
        flags |=  DBUS_WATCH_WRITABLE;
    }
    if (dbus_watch_handle (watch, flags) == FALSE)
        DWARN ("dbus_watch_handle");

	while (DBUS_DISPATCH_DATA_REMAINS == 
        dbus_connection_dispatch(connection));
    return 0;
}

void toggle_watch (DBusWatch *watch, void *data)
{
    armour_t *self = data;
    armour_evdata *event;
    struct epoll_event ev;
    uint32_t epoll_flags = 0;

    event = dbus_watch_get_data (watch);
    if (!event)
        return;

    if (TRUE == dbus_watch_get_enabled (watch)) {
        unsigned int flags = dbus_watch_get_flags(watch); 
        DPRINT ("watch is enabled!");

        if (flags & DBUS_WATCH_READABLE) {
            epoll_flags |= EPOLLIN;
        }
        if (flags & DBUS_WATCH_WRITABLE) {
            epoll_flags |= EPOLLOUT;
        }
    }
        
    ev = (struct epoll_event) { epoll_flags, { .ptr = event }};
    epoll_ctl (self->epollfd, EPOLL_CTL_MOD, event->fd, &ev);
}

dbus_bool_t add_watch (DBusWatch *watch, void *data)
{
    armour_t *self = (armour_t*) data;
    armour_evdata *event;
    int fd, ret;
    struct epoll_event ev;

    if (!dbus_watch_get_enabled(watch))
        return TRUE;

    event = malloc (sizeof *event);
    if (!event)
        return FALSE;
    
    fd = dbus_watch_get_unix_fd (watch);
    *event = (struct armour_evdata) { .cb = armour_dbus_watch_handle, 
                                      .fd = fd, .user_data = watch };

    ev = (struct epoll_event) { .data = {.ptr = event} };

    DPRINT ("fd is %d", fd);
    ret = epoll_ctl (self->epollfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0) {
        DWARN ("epoll_ctl");
        free (event);
        return FALSE;
    }

    dbus_watch_set_data (watch, event, free);

    toggle_watch (watch, self);
    return TRUE;
}

void remove_watch (DBusWatch *watch, void *data)
{
    armour_t *self = (armour_t*) data;
    armour_evdata *event;
    
    event = dbus_watch_get_data (watch);
    if (!event)
        return; 

    DPRINT ("remove: fd is %d", event->fd);
    epoll_ctl (self->epollfd, EPOLL_CTL_DEL, event->fd, NULL);
}

DBusHandlerResult armour_dbus_dispatch (DBusConnection *conn, DBusMessage *msg, void *user_data)
{
    armour_t *self = user_data;
    DBusMessage *reply = NULL;

    /*
     * Introspect
     */
    if (dbus_message_is_method_call (msg, DBUS_INTERFACE_INTROSPECTABLE, "Introspect")) {
        const char *data = armourd_dbus_introspect_xml;

        reply = dbus_message_new_method_return (msg);
        dbus_message_append_args (reply, DBUS_TYPE_STRING, &data,
                                  DBUS_TYPE_INVALID);
    }

    /*
     * WatchProcess
     */
    if (dbus_message_is_method_call (msg, ARMOURD_DBUS_INTERFACE, "WatchProcess")) {
        pid_t pid;
        char *exe = NULL;
        dbus_uint32_t resultcode; 

        dbus_message_get_args (msg, NULL, DBUS_TYPE_INT32, &pid, DBUS_TYPE_INVALID);

//            reply = dbus_message_new_error_printf (msg, DBUS_ERROR_FAILED, 
//                                               "Already watching process %d", pid);
        if (armour_lookup_pid (self, pid)) {
            resultcode = ARMOUR_DBUS_REPLY_ALREADY_WATCHED;

        } else if ((exe = armour_proc_readlink (pid, "exe"))
                        && armour_add_new (self, exe)
                        && armour_add_watch (self, pid) == 0) {
            resultcode = ARMOUR_DBUS_REPLY_SUCCESS;

        } else {
            resultcode = ARMOUR_DBUS_REPLY_NOMEM;
        }

        reply = dbus_message_new_method_return (msg);
        dbus_message_append_args (reply, DBUS_TYPE_UINT32, &resultcode,
                                      DBUS_TYPE_INVALID);
    }

    /*
     * ListProcesses
     */
    else if (dbus_message_is_method_call (msg, ARMOURD_DBUS_INTERFACE, "ListProcesses")) {
        DBusMessageIter iter, array_iter;
        armour_proc *proc;
        char *dict_signature =  DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING    \
                                DBUS_TYPE_STRING_AS_STRING              \
                                DBUS_TYPE_VARIANT_AS_STRING             \
                                DBUS_DICT_ENTRY_END_CHAR_AS_STRING;

        reply = dbus_message_new_method_return (msg);
        dbus_message_iter_init_append (reply, &iter);

        dbus_message_iter_open_container (&iter, DBUS_TYPE_ARRAY, dict_signature, &array_iter);    
        
        LIST_FOREACH (self->head, proc) {

            if (proc->pid != 0) {

                DBusMessageIter dict_iter;

                dbus_message_iter_open_container (&array_iter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_iter);    
                dbus_message_iter_append_basic (&dict_iter, DBUS_TYPE_STRING, &proc->exe);

                {
                    DBusMessageIter variant_iter;

                    dbus_message_iter_open_container (&dict_iter, 
                        DBUS_TYPE_VARIANT, DBUS_TYPE_INT32_AS_STRING, &variant_iter);    
                    dbus_message_iter_append_basic (&variant_iter, DBUS_TYPE_INT32, &proc->pid);
                    dbus_message_iter_close_container (&dict_iter, &variant_iter);
                }

                dbus_message_iter_close_container (&array_iter, &dict_iter);
            }
        }

        dbus_message_iter_close_container (&iter, &array_iter);
    }
    
    if (reply) {
        dbus_connection_send (conn, reply, NULL);
        dbus_message_unref (reply);
    }

    return DBUS_HANDLER_RESULT_HANDLED; 
}

int _armour_dbus_register (armour_t *self)
{
    DBusError error;
	dbus_error_init (&error); // does not allocate any memory
    DBusConnection *connection = dbus_bus_get (DBUS_BUS_SESSION, NULL);
    DBusObjectPathVTable vtable = { 0 };

    vtable.unregister_function = NULL;
    vtable.message_function = armour_dbus_dispatch;

    dbus_connection_try_register_object_path (connection, "/com/github/armourd",
                                              &vtable, self, &error);
    if (dbus_error_is_set (&error)) {
		warnx ("dbus: %s: %s", error.name, error.message);
        dbus_error_free (&error);
        return -1;
    }

    return 0;
}

static int _armour_dbus_set_dispatch_event_notifier (armour_t *self)
{
    armour_evdata *event = NULL;
    int fd;
    struct epoll_event ev;
    DBusConnection *connection = dbus_bus_get (DBUS_BUS_SESSION, NULL);

    if ((fd = eventfd (0, EFD_CLOEXEC)) < 0)
        return -1;

    event = malloc (sizeof *event);
    if (!event)
        return -1;

    *event = (struct armour_evdata) { .cb = armour_dbus_dispatch_handle, 
                                     .fd = fd, .user_data = connection };

    ev = (struct epoll_event) { EPOLLIN, { .ptr = event } };

    if (epoll_ctl (self->epollfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        free (event);
        return -1;
    }

    dbus_connection_set_dispatch_status_function (connection, 
        dispatch_status_changed, event, free);

    return 0;
}

int armour_dbus_init (armour_t *self)
{
    DBusError error;
	dbus_error_init (&error); // does not allocate any memory
    DBusConnection *connection;

	connection = dbus_bus_get (DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set (&error)) {
		warnx ("dbus: %s: %s", error.name, error.message);
        dbus_error_free (&error);
        return -1;
    }

	if (dbus_bus_request_name (connection, "com.github.Armourd", 
                               0, &error) < 0) {
		warnx ("dbus: %s: %s", error.name, error.message);
		dbus_error_free (&error);
		return -1;
	}

    if (_armour_dbus_register (self) < 0) {
        return -1;
    }

    if (dbus_connection_set_watch_functions (connection, add_watch,
        remove_watch, toggle_watch, self, NULL) == FALSE) {
        return -1;
    }

    if (dbus_connection_set_timeout_functions (connection, add_timer,
        remove_timer, toggle_timer, self, NULL) == FALSE) {
        return -1;
    }

    if (_armour_dbus_set_dispatch_event_notifier (self) < 0)
        return -1;

    return 0;
}

//int main (void)
//{
//    armour_t self;
//    if (armour_dbus_init(&self) < 0)
//        err (1, "not working");
//
//    while (dbus_connection_read_write_dispatch (self.conn, -1))
//    ;
//
//    return 0;
//}

