bin_PROGRAMS = armourd
armourd_SOURCES = 		\
	armour.c 			\
	armour_proc.c 		\
	armour_config.c 	\
	armour_watchdog.c	\
	main.c

if DBUS
armourd_SOURCES += dbus/armour_dbus.c
endif

armourd_LDADD = $(DBUS_LIBS)
AM_CPPFLAGS = -DSYSCONFDIR=\"${sysconfdir}\" -I./dbus #-D_FORTIFY_SOURCE=0
AM_CFLAGS = -Wall -fms-extensions -Wno-unused-result -std=gnu99 $(DBUS_CFLAGS)
sysconf_DATA = armourd.conf

