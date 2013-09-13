bin_PROGRAMS = armourd
armourd_SOURCES = 	\
	src/armour.c 	\
	src/proc.c 		\
	src/config.c 	\
	src/watchdog.c	\
	src/log.c		\
	src/main.c

if DBUS
armourd_SOURCES += dbus/armour_dbus.c
endif

armourd_LDADD = $(DBUS_LIBS)
AM_CPPFLAGS = -DSYSCONFDIR=\"${sysconfdir}\" -I./src -I./src/dbus #-D_FORTIFY_SOURCE=0
AM_CFLAGS = -Wall -fms-extensions -Wno-unused-result -std=gnu99 $(DBUS_CFLAGS)
sysconf_DATA = armourd.conf

