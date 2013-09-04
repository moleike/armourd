armourd
=======

armourd is a Linux daemon for service recovery and is targeted for use in
embedded systems running GNU/Linux.

When a watched application crashes, armourd will automatically restart it. To
watch an application armourd only needs the file path of the application
executable; no PID files, no shell scripts. This is achieved by jointly
tracking the creation/termination of processes and collecting process
information.

It works out of the box, as it takes roughly a couple of minutes to set it up.
Upon startup armourd interprets its configuration file `/etc/armourd.conf`,
which lists the pathnames of the services that need to be watched.

armourd recovers processes that stopped abnormally, by returning a nonzero exit
code; it does not recover applications hanging. Forking servers will have their
children terminate before restart. Although it is designed to recover system
daemon programs, it can actually recover any application, with the limitation
that currently only supports single-instance applications unless using the DBus
interface (see below).

armourd is Linux-only by design, because it relies upon interfaces such as
[epoll(7)](http://man7.org/linux/man-pages/man7/epoll.7.html) or [netlink(7)](http://man7.org/linux/man-pages/man7/netlink.7.html) 
and [proc(5)](http://man7.org/linux/man-pages/man5/proc.5.html) 
files that aren't available in other unices.
It is more suited to sysvinit-based systems; newer init systems, such as
[upstart](http://upstart.ubuntu.com/) or [systemd](http://www.freedesktop.org/wiki/Software/systemd/), 
service recovery is readily available.

Goals
-----

* simplest possible configuration
* no library dependencies (other than the C library)
* small memory footprint
* fast recovery of processes


Usage
-----

Populate the config file with the (absolute) pathnames of your applications,
e.g.:

        $ echo '/opt/foo' > /etc/armourd.conf

You can also use file-name globs ('*' wildcard). For example, to add all the
binaries under /usr/local/bin/:

        $ echo '/usr/local/bin/*' > /etc/armourd.conf

Additionally, you may explicitly request to watch a running process, using
DBus, e.g. with `dbus-send` you would:

        $ dbus-send --session --print-reply --type=method_call \
        > --dest=com.github.Armourd /com/github/armourd \
        > com.github.Armourd.WatchProcess int32:`pidof foo`

Or, using the provided example Python script:

        $ ./client-example.py -w`pidof foo`


D-Bus Interface
--------------

Optionally, you can build the daemon with [D-Bus](http://dbus.freedesktop.org/) 
support to commuincate with other applications. The D-Bus API exposes runtime 
information, and a method to watch running processes. The introspection XML data is:
```xml
<node>
<interface name="org.freedesktop.DBus.Introspectable">
<method name="Introspect">
<arg name="xml_data" type="s" direction="out"/>
</method>
</interface>
<interface name="com.github.Armourd">
<method name="ListProcesses">
<arg type="a{sv}" direction="out"/>
</method>
<method name="WatchProcess">
<arg name="pid" type="i" direction="in"/>
<arg name="resultcode" type="u" direction="out"/>
</method>
</interface>
</node>
```



Testsuite
---------

Current version is not production ready, as there aren't any tests.  Unit tests
to exercise the API's and integration tests to check the runtime behaviour are
a must.  This could be added as a configure option (e.g. --enable-tests) then
the user can run the make *check* target

Notes
-----

armourd just does individual service recovery; it does not perform system
recovery, i.e. talk to watchdog hardware.  In order to make your system fully
reliable, you should have a process feeding a watchdog, although your mileage
may vary.  Linux provides a very simple interface to watchdog timers
(`/dev/watchdog`) that armourd will implement as a non-default feature, to
provide an all-round solution.

TODO
----

* Testsuite
* Logging
* Configuration file parser needs to be rewritten (it is buggy)
* Enable options in the configuration file and DBus interface to tailor
  specific needs, e.g. return codes for success or a recover script
* watchdog capabilities
