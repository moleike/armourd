/* vim: set sw=4 ts=4 et : */
/* armour_config.c: parser for armourd.conf file
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
/*
 *  ad-hoc parser for armourd configuration file
 *  TODO : there is a lot of redundancy, it is just a first (last?) implementation!
 *  TODO : high priority to be rewritten
 */
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#include "armour.h"
#include "armour_proc.h"

// if is a symlink returns the contents in linkname
static int check_file (const char *filepath, char **linkname)
{
    struct stat sb;
    char *buf = NULL;
    ssize_t ret;

    if (lstat (filepath, &sb) == -1)
        return -1;

    if (S_ISLNK (sb.st_mode)) {
        // dereference it
        buf = malloc (sb.st_size + 1);
        if (buf == NULL)
            return -1;
        ret = readlink (filepath, buf, sb.st_size + 1);
        if (ret == -1) {
            free (buf);
            return -1;
        }
        // TODO handle race condition between calls
        buf[sb.st_size] = '\0';
        
        /* return here the dereferenced link */
        *linkname = buf;

        stat (buf, &sb);
    }

    /* make sure is an executable file */
    if (!((S_ISREG (sb.st_mode)) 
    && (sb.st_mode & (S_IRUSR | S_IXUSR)))) { 
        free (buf);
        return -1;
    }

    return 0;
}

int armour_config_read (armour_t *self, const char *filepath)
{
    char *str, *fstart;
    int fd, flen;
    struct stat sb;
    enum { DEFAULT, ERROR } state;  

    fd = open (filepath, O_RDONLY);
    if (fd < 0) {
        warn ("open %s", filepath);
        return -1;
    }

    if (fstat (fd, &sb) < 0) {
        warn ("fstat %s", filepath);
        return -1;
    }

    flen = sb.st_size; 

    if (flen == 0)
        return 0;

    str = mmap (NULL, flen, PROT_READ, MAP_PRIVATE, fd, 0);
    if (str == MAP_FAILED) {
        warn ("mmap %s", filepath);
        return -1;
    }

    fstart = str;

    //TODO lock the file for reading!

    state = DEFAULT;

    while (str - fstart < flen) {
        char *start, *filepath = NULL, *linkname = NULL;
        // TODO so far we discard the options
        armour_options options = { 0 };

        switch (state) {
            case DEFAULT:
            default:
                if (isspace (*str)) {
                    while (isspace (*++str));
                    break;
                }
                else if (*str == '#') {
                    while (*str++ != '\n');
                    break;
                }
                /* new entry */
                else /* if (*str == '/') */{
                    start = str;
                    while (!isspace (*str) && *str != '{' && *str++ != '*');

                    /* we only like absolute pathnames */
                    if (*start != '/') {
                        //errno = EINVAL;
                        //warn ("file %s", filepath);
                        state = ERROR;
                        break;
                    }
                    /* parse the filepath */
                    filepath = strndup (start, str - start); // null byte added

                    
                    if (*(str - 1) == '*') {
                        /* globbing pathname */
                        glob_t gl;

                        if (glob (filepath, 0, NULL, &gl) == 0) {
                            size_t cc = gl.gl_pathc;
                            while (cc--) {
                                char *file = NULL;
                                if (check_file (gl.gl_pathv[cc], &file) == 0) {
                                        armour_add_new (self, file? file: gl.gl_pathv[cc]);
                                        free (file);
                                }
                            }
                        } else {
                            state = ERROR;
                        }
                        globfree (&gl);
                        free (filepath);
                        break;   
                    }

                    if (check_file (filepath, &linkname) < 0) {
                        free (filepath);
                        state = ERROR;
                        break;
                    }
                    while (isspace (*++str));
                    
                    if (*str == '{') {
                        str++;
                    /* optional parameters for 'filepath' */
                    while (*str != '}') {
                        char *optag = NULL, **opval = NULL;

                        while (isspace (*str)) str++;
                        start = str;
                        while (!isspace (*str) && *str != ':')
                            str++;

                        optag = strndup (start, str - start);

                        if (strcmp (optag, "pidfile") == 0)
                            opval = &options.pidfile; 
                        else if (strcmp (optag, "command") == 0)
                            opval = &options.command; 
                        else if (strcmp (optag, "notify") == 0)
                            opval = &options.notify; 
                        else {
                            warn ("wrong option: %s", optag);
                            state = ERROR;
                        break;
                        }
                        while (isspace (*++str));
                            if (*str++ != ':') {
                                free (optag);
                                state = ERROR;
                                break;
                            }
                        
                        while (isspace (*++str));
                            start = str;
                        
                        if (*str == '"') {
                        start++;
                        while (*++str != '"')
                            if (*str == '\\') str++;
                        } else {
                        while (!isspace (*++str));
                        }

                        *opval = strndup (start, str - start);
                        free (optag);
                        
                        while (isspace (*++str));
                        continue;
                    }
                    str++;
                    }
                    if (state != ERROR) {
                        /* store entry */
                        if (linkname) {
                            free (filepath);
                            filepath = linkname;
                        }
                        armour_add_new (self, filepath);
                        free (filepath);
                        state = DEFAULT;
                    }
                    break;
                }
            case ERROR:
                state = DEFAULT;
                while (isspace (*str)) str++;
                if (*str != '{')
                    break;
                while (*str++ != '}');
                break;
            }
    }

    return 0;
}
