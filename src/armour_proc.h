/* vim: set sw=4 ts=4 et : */
/* armour_proc.h: proc(5) functions
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
#ifndef _ARMOUR_PROC_H
#define _ARMOUR_PROC_H

#include <sys/types.h>
/* 
 * Stores all data needed to recovery as found in proc/<pid>/ entries,
 * it can't keep track of everything (e.g. umask), but some of this is
 * indeed redundant.
 * In certain cases it'd be better to pass a shell script to call
 * to recover (options->script)
 */
struct proc_attr 
{
    pid_t pid;      /**< current pid; 0: not running */
    char *comm;     /**< filename for logging */
    uid_t uid;      /**< user id */
    gid_t gid;      /**< group id */
    char **cmdline; /**< arguments */
    char **environ; /**< environment */
    char *cwd;      /**< working directory */
    char *root;     /**< root directory */
    char *file[3];  /**< stdin, stdout and stderr files */
    gid_t *gidlist; /**< TODO: supplementary group IDs */
};

/*
 * these are options that could be passed from the configuration file
 * and aren't implemented yet.
 * the <pidfile> option is redundant since armourd currently only allows
 * one instance per service.
 * the <command> option should be for very specific processes that need
 * to do things that we cannot infer from the proc files.
 *
 */
struct armour_options
{
    char *pidfile;  /**< specify a pidfile; */
    char *command;  /**< use this shell command to recover */
    char *notify;   /**< run some other command when crashes */
};

struct armour_proc 
{
    char *exe;                      /**< application pathname */
    unsigned short flags;           /**< additional flags */
    struct proc_attr;               /**< entries from /proc/<pid> */
    struct armour_options options;  /**< TODO: optional parameters */
    struct armour_proc *next;
};

/* per-process flags */
#define ARPROC_SETSID       (1 << 0)    /**< needs setsid(2) */
#define ARPROC_WATCHED      (1 << 1)    /**< the process is running */
#define ARPROC_SCRIPT       (1 << 2)    /**< has a recover shell script */
#define ARPROC_PIDFILE      (1 << 3)    /**< has a pidfile */
#define ARPROC_VERBOSE      (1 << 4)    /**< be verbose when recovering */
#define ARPROC_RUNNING      (1 << 5)    
#define ARPROC_RECOVERING   (1 << 6)    

typedef struct  armour_proc                     armour_proc;
typedef struct  armour_options                  armour_options;
typedef int     armour_proc_func                (armour_proc *, void *);

armour_proc    *armour_proc_new                 (const char *filename, armour_options *op);
int             armour_proc_delete              (armour_proc *p, void *data);
int             armour_proc_set_param           (armour_proc *p, pid_t pid);
void            armour_proc_free_param          (armour_proc *p);
int             armour_proc_set_param_environ   (armour_proc *p, void *data);
int             armour_proc_set_param_cmdline   (armour_proc *p, void *data);
int             armour_proc_set_param_exe       (armour_proc *p, void *data);
int             armour_proc_set_param_io        (armour_proc *p, void *data);
int             armour_proc_set_param_cwd       (armour_proc *p, void *data);
int             armour_proc_set_param_root      (armour_proc *p, void *data);
int             armour_proc_dump                (armour_proc *p, void *data);
int             armour_proc_recover             (armour_proc *p, void *data);
char           *armour_proc_readlink            (pid_t pid, const char *filename);
char           *armour_proc_read                (pid_t pid, const char *filename);
pid_t           armour_proc_getppid             (pid_t pid, void *data);

#endif //_ARMOUR_PROC_H
