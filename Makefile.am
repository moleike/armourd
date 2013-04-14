bin_PROGRAMS = armourd
armourd_SOURCES = main.c armour.c armour_proc.c armour_config.c
AM_CPPFLAGS = -DSYSCONFDIR=\"${sysconfdir}\" #-D_FORTIFY_SOURCE=0
AM_CFLAGS = -Wall -Werror -fms-extensions -Wno-unused-result
sysconf_DATA = armourd.conf

