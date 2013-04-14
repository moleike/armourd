/* vim: set sw=4 ts=4 et : */
/* armour.c: process tracking
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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/cn_proc.h>
#include <linux/filter.h>
#include <arpa/inet.h>

#include "debug.h"
#include "armour_proc.h"
#include "armour.h"

void armour_add (armour_t *self, armour_proc *item)
{
    item->next = self->head;
    self->head = item;
}

void armour_rem (armour_t *self, armour_proc *item)
{
    armour_proc *p, **pp = &self->head;

    for (p = self->head; p != NULL; p = p->next) {
        if (p == item) {
            *pp = p->next;
            p->next = NULL;
            return;
        }
	    pp = &p->next;
    }
}

void armour_apply_foreach (armour_t *self, armour_proc_func *func, void *data)
{
    armour_proc *p, *n;

    for (p = self->head; p != NULL; p = n) {
        n = p->next;
        func (p, data);
    }
}

armour_proc *armour_lookup (armour_t *self, armour_proc_func *func, void *data)
{
    armour_proc *p;

    for (p = self->head; p != NULL; p = p->next)
        if ((*func) (p, data))
            return p;

    return NULL;
}

int _armour_compare_pid (armour_proc *p, void *data)
{
    pid_t pid = (pid_t)(intptr_t)data;

    if (p->pid == pid)
        return 1;
    return 0;

}

int _armour_compare_path (armour_proc *p, void *data)
{
    char *exe = (char *)data;

    if (strcmp (p->exe, exe) == 0)
        return 1;
    return 0;

}

/*
 * returns true if there is another entry of p->exe not initialized
 */
int _armour_get_next_free (armour_proc *p, void *data)
{
    armour_proc *q = (armour_proc *)data;

    if ((strcmp (p->exe, q->exe) == 0)
    && (0 == p->pid)) {
        return 1;
    }
    return 0;
}

/*
 * returns true if there is another entry of p->exe
 */
int _armour_get_next (armour_proc *p, void *data)
{
    armour_proc *q = (armour_proc *)data;

    if ((strcmp (p->exe, q->exe) == 0)
    && (q->pid != p->pid)) {
        return 1;
    }
    return 0;
}

void armour_print_all_procs (armour_t *self)
{
    armour_apply_foreach (self, armour_proc_dump , NULL);
}

void armour_delete_all_procs (armour_t *self)
{
    armour_apply_foreach (self, armour_proc_delete, NULL);
}

int armour_listen (armour_t *self)
{
    struct iovec iov[3];
    char buf[NLMSG_HDRLEN];
    struct nlmsghdr *nlmsghdr = (struct nlmsghdr*)buf;
    struct cn_msg cn_msg;
    enum proc_cn_mcast_op op;

    // netlink header
    *nlmsghdr = (struct nlmsghdr) { NLMSG_LENGTH (sizeof cn_msg + sizeof op), 
        NLMSG_DONE, 0, 0, getpid() };
    iov[0] = (struct iovec) { buf, NLMSG_LENGTH (0) };

    // connector
    cn_msg = (struct cn_msg) {{CN_IDX_PROC, CN_VAL_PROC}, 0, 0, sizeof op}; 
    iov[1] = (struct iovec) { &cn_msg, sizeof cn_msg };

    // proc connector
    op = PROC_CN_MCAST_LISTEN; /* to stop it PROC_CN_MCAST_IGNORE */
    iov[2] = (struct iovec) { &op, sizeof op };

    if (writev (self->sock, iov, sizeof iov / sizeof iov[0]) < 0)
        return -1;

    return 0;
}

int armour_filter (armour_t *self)
{
    struct sock_fprog fprog;
    struct sock_filter filter[] = {
        BPF_STMT (BPF_LD + BPF_H + BPF_ABS, offsetof (struct nlmsghdr, nlmsg_type)),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, htons (NLMSG_DONE), 1, 0),
        BPF_STMT (BPF_RET | BPF_K, ~(0L)),
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, id) 
                  + offsetof (struct cb_id, idx)) ),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, htonl (CN_IDX_PROC), 1, 0),
        BPF_STMT (BPF_RET | BPF_K, ~(0L)),
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, id) 
                  + offsetof (struct cb_id, val)) ),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, htonl (CN_VAL_PROC), 1, 0),
        BPF_STMT (BPF_RET | BPF_K, ~(0L)),
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, data) 
                  + offsetof (struct proc_event, what)) ),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, htonl (PROC_EVENT_EXEC), 0, 6),
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, data) 
                  + offsetof (struct proc_event, event_data)
                  + offsetof (struct exec_proc_event, process_pid))),
        BPF_STMT (BPF_MISC + BPF_TAX, 0),
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, data) 
                  + offsetof (struct proc_event, event_data)
                  + offsetof (struct exec_proc_event, process_tgid))),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_X, 0, 1, 0),
        BPF_STMT (BPF_RET | BPF_K, 0),
        BPF_STMT (BPF_RET | BPF_K, ~(0L)),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, htonl (PROC_EVENT_EXIT), 0, 6),
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, data) 
                  + offsetof (struct proc_event, event_data)
                  + offsetof (struct exit_proc_event, process_pid))),
        BPF_STMT (BPF_MISC + BPF_TAX, 0),
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, data) 
                  + offsetof (struct proc_event, event_data)
                  + offsetof (struct exit_proc_event, process_tgid))),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_X, 0, 1, 0),
        BPF_STMT (BPF_RET | BPF_K, 0),
        BPF_STMT (BPF_RET | BPF_K, ~(0L)),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_K, htonl (PROC_EVENT_SID), 1, 0),
        BPF_STMT (BPF_RET | BPF_K, 0),  /* other proc_event: drop it */
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, data) 
                  + offsetof (struct proc_event, event_data)
                  + offsetof (struct sid_proc_event, process_pid))),
        BPF_STMT (BPF_MISC + BPF_TAX, 0),
        BPF_STMT (BPF_LD + BPF_W + BPF_ABS, (NLMSG_LENGTH (0) 
                  + offsetof (struct cn_msg, data) 
                  + offsetof (struct proc_event, event_data)
                  + offsetof (struct sid_proc_event, process_tgid))),
        BPF_JUMP (BPF_JMP + BPF_JEQ + BPF_X, 0, 1, 0),
        BPF_STMT (BPF_RET | BPF_K, 0), 
        BPF_STMT (BPF_RET | BPF_K, ~(0L)),
    };

    fprog.filter = filter;
    fprog.len = sizeof filter / sizeof filter[0]; /* number of filter blocks */

    setsockopt (self->sock, SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof fprog);

    return 0;
}

int armour_handle (armour_t *self)
{
    struct sockaddr_nl addr;
    struct msghdr msghdr;
    struct nlmsghdr *nlmsghdr;
    struct iovec iov[1];
    char buf[4096];
    ssize_t len;

    msghdr = (struct msghdr) { &addr, sizeof addr, iov, 1, NULL, 0, 0 };
    iov[0] = (struct iovec) { buf, sizeof buf };

    len = recvmsg (self->sock, &msghdr, 0);
    if (len < 0)
        return -1;

    if (addr.nl_pid != 0)
        return 0;
    
    for (nlmsghdr = (struct nlmsghdr *)buf;
         NLMSG_OK (nlmsghdr, len);
         nlmsghdr = NLMSG_NEXT (nlmsghdr, len)) {

        struct cn_msg *cn_msg;
        struct proc_event *ev;

        if ((NLMSG_ERROR == nlmsghdr->nlmsg_type)
        || (NLMSG_NOOP == nlmsghdr->nlmsg_type))
            continue;

        /* payload of netlink msg: connector msg */
        cn_msg = NLMSG_DATA (nlmsghdr);

        if ((cn_msg->id.idx != CN_IDX_PROC)
        || (cn_msg->id.val != CN_VAL_PROC))
            continue;
        
        /* payload of connector msg: proc_event msg */
        ev = (struct proc_event *)cn_msg->data;
        
        switch (ev->what) {
            /*
             * tgid is the process id
             * pid is the thread id
             * these are the same here, as we filter all thread events
             */
            case PROC_EVENT_EXIT:
                if (0 == ev->event_data.exit.exit_code) {
                    armour_remove_watch (self, 
                        ev->event_data.exit.process_pid);
                } else {
                    armour_recover (self, 
                        ev->event_data.exit.process_pid);
                }
                break;
            case PROC_EVENT_EXEC:
                armour_add_watch (self, 
                    ev->event_data.exec.process_pid);
                break;
            case PROC_EVENT_SID:
                armour_update_watch (self, 
                    ev->event_data.sid.process_pid);
                break;
            default:
                break;
        }
    }
    return 0;
}

int _proc_pid_filter (const struct dirent *dirent)
{
    char *endptr;

    if (DT_DIR == dirent->d_type) {
        (void)strtol (dirent->d_name, &endptr, 10);
        if ('\0' == *endptr) // d_name contains only digits
            return 1;
    }
    return 0;
}

int armour_init (armour_t **handle, const char *config_file)
{
    armour_t *self;
    int n;
    struct dirent **namelist = NULL;
    struct sockaddr_nl addr;
    sigset_t sigmask;
    int signals[] = {SIGTERM, SIGINT, SIGQUIT, SIGCHLD};

    self = malloc (sizeof *self);
    if (!self)
	    return -1;
    memset (self, 0, sizeof *self);

    if (config_file)
        if (armour_config_read (self, config_file) < 0)
            return -1;

    n = scandir ("/proc", &namelist, _proc_pid_filter, versionsort);
    if (n < 0)
        return -1;
    else {
        while (n--) {
            armour_proc *p;
            long pid;
            char *exe;

            pid = strtol (namelist[n]->d_name, NULL, 10);
            exe = armour_proc_readlink (pid, "exe");
            if (!exe)
                continue;

            p = armour_lookup (self, _armour_compare_path, (void*)exe);
            
            if (p) {
                if (armour_proc_set_param (p, pid) == -1) {
                    /* failed to initialize */
                    warn ("failed to track %s", p->exe);
		        }
            }
            free (namelist[n]);
            free (exe);
        }
        free (namelist);
    }
    
    /* 
     * signals
     */
    sigemptyset (&sigmask);
    
    for (n = 0; n < (sizeof signals / sizeof signals[0]); n++)
        sigaddset (&sigmask, signals[n]);
    
    sigprocmask (SIG_BLOCK, &sigmask, NULL);

    self->signalfd = signalfd (-1, &sigmask, SFD_CLOEXEC);
    if (signalfd < 0)
        return -1;

    /*
     * netlink socket
     */
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid ();
    addr.nl_groups = CN_IDX_PROC;

    self->sock = socket (PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, // SOCK_NONBLOCK 
                         NETLINK_CONNECTOR);

    bind (self->sock, (struct sockaddr *)&addr, sizeof addr);

    armour_filter (self);

    /*
     * epoll
     */
    self->epollfd = epoll_create1 (EPOLL_CLOEXEC);
    if (self->epollfd < 0)
        return -1;

    *handle = self; /* return instance to the caller */
    return 0;
}

int armour_run (armour_t *self)
{
    struct epoll_event ev[2], events[2];
    struct signalfd_siginfo siginfo;
    int nfds;
    int ret; /* 0: success, 1: failure */

    ret = armour_listen (self);
    if (ret < 0)
        return 1;
  
    ev[0] = (struct epoll_event) { EPOLLIN, { .fd = self->signalfd } };
    ret = epoll_ctl (self->epollfd, EPOLL_CTL_ADD, self->signalfd, &ev[0]);
    if (ret < 0)
        return 1;

    ev[1] = (struct epoll_event) { EPOLLIN, { .fd = self->sock } };
    ret = epoll_ctl (self->epollfd, EPOLL_CTL_ADD, self->sock, &ev[1]);
    if (ret < 0)
        return 1;

    /*
     * not used yet, but will be when we need
     * to do cleanups after the loop breaks
     */
    self->running = 1;
    
    while (self->running) {
        nfds = epoll_wait (self->epollfd, events, 1, -1);

        if (nfds < 0) {
            if (EINTR == errno)
                continue;
            else
                return 1;
        }
 
        while (nfds--) {
            if (events[nfds].data.fd == self->signalfd) {
                ret = read (self->signalfd, &siginfo, sizeof siginfo);
                if (ret < 0)
                    return 1;

                if (SIGCHLD == siginfo.ssi_signo) {
                    pid_t pid;
                    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
                        DPRINT ("child %d exited", pid);
                } else {
                    psignal (siginfo.ssi_signo, "armourd");
                    return 0;
                }
            }

            if (events[nfds].data.fd == self->sock) {
                ret = armour_handle (self);
                if (ret < 0)
                    return 1;
            }
        }
    }
    return 0;
}

int armour_remove_watch (armour_t *self, pid_t pid)
{
    armour_proc *p;
    
    p = armour_lookup (self, _armour_compare_pid, (void*)(intptr_t)pid);
    if (p) {
        DPRINT ( "removing watch from %s", p->exe);
        p->flags &= ~ARPROC_WATCHED;
        armour_proc_free_param (p);
    }
    return 0;
}

int armour_recover (armour_t *self, pid_t pid)
{
    armour_proc *p;
    
    p = armour_lookup (self, _armour_compare_pid, (void*)(intptr_t)pid);
    if (p){
        if (armour_proc_recover (p, NULL) < 0) {
            DPRINT ("cannot recover %s", p->exe);
            return -1;
        }
        DPRINT ("recovered %s", p->exe);
    }
    return 0;
}

int armour_update_watch (armour_t *self, pid_t pid)
{
    armour_proc *p;
    char *exe;

    exe = armour_proc_readlink (pid, "exe");
    if (!exe) {
        DWARN ("readlink");
        return -1;
    }

    p = armour_lookup (self, _armour_compare_path, (void*)exe);
    free (exe);

    if (!p)
        return 0;

    if (p->flags & ARPROC_WATCHED)
        return 0;

    if (p->pid != 0) {
        armour_proc_free_param (p);
    }
    if (armour_proc_set_param (p, pid) == -1) {
        DWARN ("failed to track %s", p->exe);
        return -1;
    }

    /* remove the flag ARPROC_SETSID, as this process
     * setsid()'s itself */
    p->flags &= ~ARPROC_SETSID;

    DPRINT ("updated watch for %s", p->exe);
    return 0;
}

int armour_add_watch (armour_t *self, pid_t pid)
{
    armour_proc *p;
    char *exe;

    exe = armour_proc_readlink (pid, "exe");
    if (!exe) {
        DWARN ("readlink");
        return -1;
    }

    p = armour_lookup (self, _armour_compare_path, (void*)exe);
    free (exe);

    if (!p)
        return 0;

    if (p->pid != 0) {
        DPRINT ("already watching %s", p->exe);
        p->flags |= ARPROC_WATCHED;
        return 0;
    }

    if (armour_proc_set_param (p, pid) == -1) {
        DWARN ("failed to track %s", p->exe);
        return -1;
    }

    DPRINT ("adding watch to %s", p->exe);
    return 0;
}

