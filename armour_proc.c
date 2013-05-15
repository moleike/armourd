/* vim: set sw=4 ts=4 et : */
/* armour_proc.c: proc(5) functions
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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "debug.h"
#include "armour.h"
#include "armour_proc.h"


static int _armour_proc_open (long pid, char *filename)
{
    char *filepath = NULL;
    int fd;

    if (-1 == asprintf (&filepath,
            "/proc/%ld/%s", pid, filename)) {
        return -1;
    }

    fd = open (filepath, O_RDONLY);
    free (filepath);

    return fd;
}

char *armour_proc_readlink (long pid, char *filename)
{
    char *filepath = NULL, *buf = NULL;
    long len;
    int ret;

    len = pathconf ("/", _PC_PATH_MAX);
    /* add one since it's relative to root */
    len++;

    buf = malloc (len);
    if (!buf)
        return NULL;

    ret = asprintf (&filepath, "/proc/%ld/%s", pid, filename);
    if (ret == -1) {
        free (buf);
        return NULL;
    }
    ret = readlink (filepath, buf, len);
    if (ret == -1) {
        free (filepath);
        free (buf);
        return NULL;
    }
    buf[ret] = '\0';

    free (filepath);
    return buf;
}

/* 
 * procfs files can't be stat
 * returns a malloc()'d buffer with the file contents.
 */
#define CHUNK_SIZE (1 << 6)
char *armour_proc_read (long pid, char *filename)
{
    char *buf, *p, *q;
    ssize_t count, size, len, cc;
    int fd;

    size = CHUNK_SIZE;
    cc = 0;
    count = 0;
    len = size - count;

    fd = _armour_proc_open (pid, filename);
    if (fd < 0) {
        return NULL;
    }

    buf = malloc (size);
    if (!buf)
        return NULL;
    q = buf;

    while ((cc = read (fd, q, len)) == len) {
        size <<= 1;   // grows geometrically, which is ok for the size we expect
        count += cc;
        len = size - count;
        p = realloc (buf, size);
        if (!p) {
            free (buf);
            return NULL;
        }
        buf = p;
        q = buf + count;
    }
    count += cc;
//    if (buf[count -1] == '\n')
//        buf[count -1] = '\0';

    /*
     * fill with zeros the remaining area
     */
    memset (buf + count, 0, size - count);

    return buf;
} 

//int armour_proc_set_param_exe (armour_proc *p, void *data)
//{
//    (void)data;
//    p->exe = armour_proc_readlink (p->pid, "exe");
//    if (!p->exe) 
//        return -1;
//    return 0;
//}

int armour_proc_set_param_cwd (armour_proc *p, void *data)
{
    (void)data;
    p->cwd = armour_proc_readlink (p->pid, "cwd");
    if (!p->cwd) 
        return -1;
    return 0;
}

int armour_proc_set_param_root (armour_proc *p, void *data)
{
    (void)data;
    p->root = armour_proc_readlink (p->pid, "root");
    if (!p->root) 
        return -1;
    return 0;
}

int armour_proc_set_param_environ (armour_proc *p, void *data)
{
    char *buf = NULL, **vbuf = NULL;
    int i = 0, len;
    int nstr = 50; // FIXME
    (void)data;

    buf = armour_proc_read (p->pid, "environ");
    if (!buf) {
        return -1;
    }
    /* array of pointers to terminated strings */
    vbuf = calloc (nstr, sizeof (char *));
    if (!vbuf) {
        free (buf);
        return -1;
    }

    /* buf contains a set of 
     * null-separated strings (see proc(5)) */
    while ((len = strlen (buf)) && i < nstr - 1) {
        vbuf[i++] = buf;
        buf += len + 1;
    }
    /* last string must be a NULL pointer (see execve(2)) */
    vbuf[i] = (char *)NULL;
    
    p->environ = vbuf;
    
    return 0;
}

int armour_proc_set_param_cmdline (armour_proc *p, void *data)
{
    char *buf = NULL, **vbuf = NULL;
    int i = 0, len;
    int nstr = 50; // FIXME
    (void)data;

    buf = armour_proc_read (p->pid, "cmdline");
    if (!buf) {
        return -1;
    }
    /* array of pointers to terminated strings */
    vbuf = calloc (nstr, sizeof (char *));
    if (!vbuf) {
        free (buf);
        return -1;
    }

    /* buf contains a set of 
     * null-separated strings (see proc(5)) */
    while ((len = strlen (buf)) && i < nstr - 1) {
        vbuf[i++] = buf;
        buf += len + 1;
    }
    /* last string must be a NULL pointer (see execve(2)) */
    vbuf[i] = (char *)NULL;
    
    p->cmdline = vbuf;
    
    return 0;
}

int armour_proc_set_param_comm (armour_proc *p, void *data)
{
    FILE *fp = NULL;
    int ret = 0;
    (void)data;

    /*
     * comm file isn't implemented in older kernels
     */
    //p->comm = armour_proc_read (p->pid, "comm");
    //if (!p->comm) 
    //    return -1;

    fp = fdopen (_armour_proc_open (p->pid, "stat"), "r");
    if (!fp)
        return -1;

    if (1 > fscanf (fp, "%*d (%m[^)])", &p->comm)) {
        ret = -1;
    }

    fclose (fp);
    return ret;
}

int armour_proc_set_param_io (armour_proc *p, void *data)
{
    (void)data;
    char file[5]; // fd/x
    int i;

    for (i = 0; i < 3; i++) {
        snprintf (file, sizeof file, "fd/%d", i);
        p->file[i] = armour_proc_readlink (p->pid, file);
        if (!p->file[i]) 
            return -1;
    }
    return 0;
}

int armour_proc_set_param_ugid (armour_proc *p, void *data)
{
    FILE *fp = NULL;
    char tag[64], value[64];
    long uid, gid;

    fp = fdopen (_armour_proc_open (p->pid, "status"), "r");
    if (!fp)
        return -1;

    while (2 == fscanf (fp, "%[^:]: %[^\n]\n", tag, value)) {
        if (strcmp ("Uid", tag) == 0) {
            if (1 == sscanf (value, "%ld ", &uid))
                p->uid = (uid_t) uid;
        }
        else if (strcmp ("Gid", tag) == 0) {
            if (1 == sscanf (value, "%ld ", &gid))
                p->gid = (gid_t) gid;
        }
    }        

    fclose (fp);
    return 0;
}

/*
 * TODO: set the supplementary group vector to 
 * the list of groups the user is in. 
 * use setgroups(2) and get Groups entry from /proc<pid>/status.
 */
int armour_proc_set_param (armour_proc *p, long pid)
{
    p->pid = pid;
    errno = 0;

    armour_proc_set_param_environ(p, NULL);
    armour_proc_set_param_cmdline (p, NULL);
    armour_proc_set_param_cwd (p, NULL);
    armour_proc_set_param_root (p, NULL);
    armour_proc_set_param_io (p, NULL);
    armour_proc_set_param_ugid (p, NULL);
    armour_proc_set_param_comm (p, NULL);
    
    /* seems redundant but do we need to check too pgid ? */ 
    if (pid == getsid (pid)) {
        p->flags |= ARPROC_SETSID;
    }
    // TODO: remove. This is unnecessary once we add 
    // syslog or whatever logging facility
    // plus doesnt need to be a per-process feature
#ifdef DEBUG
    p->flags |= ARPROC_VERBOSE;
#endif
    
    if (errno) {
        /* something went wrong */
        armour_proc_free_param (p);
        return -1;
    }

    if (p->flags & ARPROC_VERBOSE)
        armour_proc_dump (p, NULL);

    return 0;
}

void armour_proc_free_param (armour_proc *p)
{
    /* exe is not free'd */

    if (p->cmdline) {
        free (*p->cmdline);
        free (p->cmdline);
    }
    if (p->environ) {
        free (*p->environ);
        free (p->environ);
    }
    free (p->comm);
    free (p->root);
    free (p->cwd);
    free (p->file[0]);
    free (p->file[1]);
    free (p->file[2]);
    //free (p->options); // TODO

    /* it masks errors such double free's, 
     * so eventually should be removed */
    memset ((char*)p + offsetof (armour_proc, pid), 
            0, sizeof (struct proc_attr));
}

armour_proc *armour_proc_new (char *filename, armour_options *op)
{
    armour_proc *newp;

    newp = malloc (sizeof *newp);
    if (!newp) {
        return NULL;
    }
    memset (newp, 0, sizeof *newp);

    newp->exe = strdup (filename);
    if (!newp->exe) {
        free (newp);
        return NULL;
    }

    if (op)
        newp->options = *op;
    
    return newp;
}

int armour_proc_delete (armour_proc *p, void *data)
{
    (void)data;
    free (p->exe);
    armour_proc_free_param (p);
    free (p);
    return 0;
}

#define ARMOUR_PROC_PRINT_ATTR(x)           \
    do {                                    \
        fprintf (stderr, "%15s", #x+6);     \
        fprintf (stderr, ": %s\n", x);      \
    } while (0);


int armour_proc_dump (armour_proc *proc, void *data)
{
    (void)data;
    ARMOUR_PROC_PRINT_ATTR (proc->exe);
    if (proc->pid) {
        fprintf (stderr, "%15s: %d\n","pid", proc->pid);
        fprintf (stderr, "%15s: %s\n","status", "running");
        ARMOUR_PROC_PRINT_ATTR (proc->comm);
        ARMOUR_PROC_PRINT_ATTR (proc->root);
        ARMOUR_PROC_PRINT_ATTR (proc->cwd);
        ARMOUR_PROC_PRINT_ATTR (proc->file[0]);
        ARMOUR_PROC_PRINT_ATTR (proc->file[1]);
        ARMOUR_PROC_PRINT_ATTR (proc->file[2]);
    }
    else { 
        fprintf (stderr, "%15s: %s\n","status", "not running");
    }
    fprintf (stderr, "\n");
    return 0;
}

int armour_proc_recover (armour_proc *proc, void *data)
{
    pid_t pid;
    sigset_t set;
    int i, fd;
    (void)data;

    //pid = vfork ();
    pid = fork ();
    
    switch (pid) {
        case -1:
            DWARN ("fork");
            return -1;
        case 0:
            if (proc->flags & ARPROC_SETSID) {
                pid = fork ();
                switch (pid) {
                    case -1:
                        DWARN ("fork");
                        return -1;
                    case 0:
                        setsid (); // TODO check return value
                        break;
                    default:
                        _exit (0);
                        break;
                }
            } else {
                setpgid (0, 0);
            }
            /*
             * remove signal mask
             */
            sigprocmask (SIG_BLOCK, NULL, &set);
            sigprocmask (SIG_UNBLOCK, &set, NULL);

            /*
             * attach file desc. 0, 1 and 2 to whatever files it used
             */
            for (i = 0; i < 3; i++) {
                fd = open (proc->file[i], O_RDWR);
                dup2 (fd, i);
            }
            /*
             * change working and root directory
             */
            chdir (proc->cwd);
            chroot (proc->root);
            /*
             * set uid and gid
             */
            setgid (proc->gid);
            setuid (proc->uid);

            execve (proc->exe, proc->cmdline, proc->environ);
            DWARN ("execve");
            _exit (127); /* exec error! */

        default:
            proc->flags &= ~ARPROC_WATCHED;
            armour_proc_free_param (proc);
            break;
    }
    return 0;
}
