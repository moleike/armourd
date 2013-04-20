/* vim: set sw=4 ts=4 et : */
/* main.c: main() for armourd
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>

#include "config.h"
#include "armour.h"

static int daemon_mode = 0;

#ifdef SYSCONFDIR
static char *config_file = SYSCONFDIR "/armourd.conf";
#else
static char *config_file = "/etc/armourd.conf";
#endif

static int running (void)
{
    int fd;
    struct flock fl;
    char buf[16];

    if ((fd = open ("/var/run/armourd.pid"
        , O_RDWR | O_CREAT, 0644)) < 0) {
	    err (EXIT_FAILURE, "open");
    }

    fl = (struct flock) { .l_type = F_WRLCK, .l_whence = SEEK_SET };
    
    if (fcntl (fd, F_SETLK, &fl) < 0) {
        if (EACCES == errno || EAGAIN == errno) {
            close (fd);
            return 1;
        }
	    err (EXIT_FAILURE, "fcntl");
    }

    fcntl (fd, F_SETFD, FD_CLOEXEC);

    ftruncate (fd, 0);
    sprintf (buf, "%ld", (long)getpid());
    write (fd, buf, strlen (buf) + 1);

    /* keep it open to hold the lock */
    return 0;
}

static void usage (void)
{
    fprintf (stderr, "%s [--daemon] [--version] [--config-file=FILE]\n"
            , PACKAGE_NAME);
    exit (0);
}

static void version (void)
{
    printf ("%s\n"
            "Copyright (C) 2013, Alexandre Moreno\n"
            "This is free software: you are free to change and redistribute it.\n"
            "There is NO WARRANTY, to the extent permitted by law.\n"
            , PACKAGE_STRING);
    exit (0);
}

static void get_options (int argc, char **argv)
{
    /* parse command-line options */
    for (;;) {
        static const struct option long_options[] = {
            {"daemon", 0, NULL, 'd'},
            {"config-file", 1, NULL, 'c'},
            {"help" , 0, NULL, 'h'},
            {"version" , 0, NULL, 'v'},
            {0, 0, 0, 0},
        };

        int c = getopt_long (argc, argv, "c:dhv", long_options, NULL);
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                daemon_mode = 1;
                break;
            case 'c':
                config_file = strdup (optarg);
                break;
            case 'v':
                version ();
                break;
            case 'h':
            case '?':
            default:
                usage ();
                break;
        }
    }
}

int main (int argc, char **argv)
{
    armour_t *self;

    get_options (argc, argv);

    if (daemon_mode
    && (daemon (0, 1) < 0)) {
        err (EXIT_FAILURE, "daemon()");
    }
    if (running ()) {
        errx (EXIT_FAILURE, "already running");
    }
    if (armour_init (&self, config_file) < 0) {
        errx (EXIT_FAILURE, "failed to start");
    }
    return armour_run (self);
}
