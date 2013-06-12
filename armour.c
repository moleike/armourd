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
#include "list.h"

static int _proc_pid_filter (const struct dirent *dirent)
{
    char *endptr;

    if (DT_DIR == dirent->d_type) {
        (void)strtol (dirent->d_name, &endptr, 10);
        if ('\0' == *endptr) // d_name contains only digits
            return 1;
    }
    return 0;
}

static int armour_listen (armour_t *self)
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

    if (writev (self->sock.fd, iov, sizeof iov / sizeof iov[0]) < 0)
        return -1;

    return 0;
}

static int armour_add_filter (armour_t *self)
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

    setsockopt (self->sock.fd, SOL_SOCKET, SO_ATTACH_FILTER, &fprog, sizeof fprog);

    return 0;
}

static int armour_nlsock_handler (int sock, unsigned int events, void *data)
{
    armour_t *self = (armour_t *) data;
    struct sockaddr_nl addr;
    struct msghdr msghdr;
    struct nlmsghdr *nlmsghdr;
    struct iovec iov[1];
    char buf[4096];
    ssize_t len;

    (void)events;

    msghdr = (struct msghdr) { &addr, sizeof addr, iov, 1, NULL, 0, 0 };
    iov[0] = (struct iovec) { buf, sizeof buf };

    len = recvmsg (sock, &msghdr, 0);
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

static int armour_signal_handler (int fd, unsigned int events, void *data)
{
    armour_t *self = (armour_t *) data;
    struct signalfd_siginfo siginfo;
    int ret;
    (void)events;

    ret = read (fd, &siginfo, sizeof siginfo);
    if (ret < 0)
        return -1;

    if (SIGCHLD == siginfo.ssi_signo) {
        pid_t pid;
        while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
            DPRINT ("child %d exited", pid);
    } else {
        psignal (siginfo.ssi_signo, "armourd");
        self->running = 0;
    }
    return 0;
}

static int armour_add_signalfd (armour_t *self)
{
    struct epoll_event ev;
    sigset_t sigmask;
    int signals[] = {SIGTERM, SIGINT, SIGQUIT, SIGCHLD};
    int ret, fd, i;

    sigemptyset (&sigmask);
    
    for (i = 0; i < (sizeof signals / sizeof signals[0]); i++)
        sigaddset (&sigmask, signals[i]);
    
    sigprocmask (SIG_BLOCK, &sigmask, NULL);

    fd = signalfd (-1, &sigmask, SFD_CLOEXEC);
    if (fd < 0)
        return -1;

    self->signal = (struct armour_evdata) { .cb = armour_signal_handler, 
                                            .fd = fd, .user_data= self };

    ev = (struct epoll_event) { EPOLLIN, { .ptr = &self->signal } };

    ret = epoll_ctl (self->epollfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0)
        return 1;

    return 0;
}

static int armour_add_nlsock (armour_t *self)
{
    struct epoll_event ev;
    struct sockaddr_nl addr;
    int sock;
    int ret;

    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid ();
    addr.nl_groups = CN_IDX_PROC;

    sock = socket (PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, // SOCK_NONBLOCK 
                         NETLINK_CONNECTOR);

    bind (sock, (struct sockaddr *)&addr, sizeof addr);

    self->sock = (struct armour_evdata) { armour_nlsock_handler, sock, self };
    ev = (struct epoll_event) { EPOLLIN, { .ptr = &self->sock } };

    ret = epoll_ctl (self->epollfd, EPOLL_CTL_ADD, sock, &ev);
    if (ret < 0)
        return 1;

    return 0;
}

int armour_run (armour_t *self)
{
    struct epoll_event events[10];
    int i, nfds;

    if (armour_listen (self) < 0)
        return 1;

    self->running = 1;

    while (self->running) {
        nfds = epoll_wait (self->epollfd, events, sizeof events, -1);

        if (nfds < 0) {
            if (EINTR == errno)
                continue;
            else
                return 1;
        }
 
        for (i = 0; i < nfds; i++) {
            armour_evdata *ev = events[i].data.ptr;

            if (ev->cb (ev->fd, events[i].events, ev->user_data) < 0)
                return 1;
        }
    }

    return 0;
}

int armour_init (armour_t **handle, const char *config_file)
{
    armour_t *self;
    int n;
    struct dirent **namelist = NULL;

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

            if ((p = armour_lookup_exe (self, exe))) {
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
    
    self->epollfd = epoll_create1 (EPOLL_CLOEXEC);
    if (self->epollfd < 0)
        return -1;

    if (armour_add_signalfd (self) < 0)
        return -1;

    if (armour_add_nlsock (self) < 0)
        return -1;

    armour_add_filter (self);

    *handle = self; /* return instance to the caller */
    return 0;
}

armour_proc *armour_add_new (armour_t *self, const char *filepath)
{
    armour_proc *p = NULL; 
    if ((p = armour_proc_new (filepath, NULL)))
        LIST_ADD (self->head, p);
    return p;
}

armour_proc *armour_lookup_pid (armour_t *self, pid_t pid)
{
    return LIST_LOOKUP (self->head, pid, pid);
}

armour_proc *armour_lookup_exe (armour_t *self, const char *path)
{
    return LIST_LOOKUP_STR (self->head, exe, path);
}

void armour_apply_foreach (armour_t *self, armour_proc_func *func, void *data)
{
    armour_proc *proc;
    LIST_FOREACH_SAFE (self->head, proc)
        (*func) (proc, data);
}

int armour_remove_watch (armour_t *self, pid_t pid)
{
    armour_proc *p;
    
    p = armour_lookup_pid (self, pid);
    if (p) {
        DPRINT ( "removing watch from %s", p->exe);
        p->flags &= ~ARPROC_RUNNING;
    }
    return 0;
}

int armour_recover (armour_t *self, pid_t pid)
{
    armour_proc *p;
    
    p = armour_lookup_pid (self, pid);
    if (p){
        if (armour_proc_recover (p, NULL) < 0) {
            DPRINT ("cannot recover %s", p->exe);
            return -1;
        } 
        p->flags |= ARPROC_RECOVERING;
        DPRINT ("recovered %s", p->exe);
    }
    return 0;
}

int armour_update_watch (armour_t *self, pid_t pid)
{
    armour_proc *proc;
    pid_t ppid;
    char *exe;

    exe = armour_proc_readlink (pid, "exe");
    if (!exe) {
        DWARN ("readlink");
        return -1;
    }
    /*
     * are we watching processes with file path 'exe'?
     */
    proc = armour_lookup_exe (self, exe);
    if (!proc) {
        free (exe);
        return 0;
    }

    /*
     * if the parent of pid process hasn't exited
     * yet, we should find it here
     */
    ppid = armour_proc_getppid (pid, NULL);
    DPRINT ("PARENT PID is: %d", ppid);
    proc = armour_lookup_pid (self, ppid);

    if (proc) {
        proc->pid = pid;
        proc->flags |= ARPROC_RUNNING; /* redundant */
        proc->flags &= ~ARPROC_SETSID; /* setsid()'s itself */
        DPRINT ("updated watch for %s", proc->exe);
        free (exe);
        return 0;
    }
    /*
     * if parent exited prior to child setsid(), not common,
     */
    LIST_FOREACH (self->head, proc) {
        if (strcmp (proc->exe, exe) == 0) {
            if (!(proc->flags & ARPROC_RUNNING)) {
                proc->pid = pid;
                proc->flags |= ARPROC_RUNNING;
                proc->flags &= ~ARPROC_SETSID;
                DPRINT ("updated watch for %s", proc->exe);
                break;
            }
        }
    }
    free (exe);
    return 0;
}

int armour_add_watch (armour_t *self, pid_t pid)
{
    armour_proc *proc;
    char *path;

    path = armour_proc_readlink (pid, "exe");
    if (!path) {
        DWARN ("readlink");
        return -1;
    }

    LIST_FOREACH (self->head, proc) {
        if (strcmp (proc->exe, path) == 0) {
            if (proc->flags & ARPROC_RECOVERING) {
                proc->pid = pid;
                DPRINT ("flags are %x", proc->flags);
                proc->flags &= ~ARPROC_RECOVERING;
                DPRINT ("recover watch to %s", proc->exe);
                break;
            } else if (!(proc->flags & ARPROC_RUNNING)) {
                armour_proc_free_param (proc);
                armour_proc_set_param (proc, pid);
                proc->flags |= ARPROC_RUNNING;
                DPRINT ("adding watch to %s", proc->exe);
                break;
            }
        }
    }

    free (path);
    return 0;
}

