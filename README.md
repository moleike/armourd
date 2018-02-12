# armourd [![Build Status](https://travis-ci.org/moleike/armourd.svg?branch=master)](https://travis-ci.org/moleike/armourd)

> fast watchdog daemon for embedded systems running GNU/Linux.

armourd restarts processes that stopped abnormally, by returning a *non-zero* 
exit status code. To watch an application armourd only needs the file 
path of the application executable; *no PID files*, *no shell scripts*.

Goals
-----

* painless configuration
* no library dependencies (other than the C library)
* small memory footprint
* fast recovery of processes

Install
-------
```
 $ ./autogen.sh
 $ mkdir build && cd build
 $ ../configure
 $ make
 $ make install
```

Config file
-----------

Holds an entry for each service that needs to be watched, e.g.:

```
# echo 'path/to/foo' > /etc/armourd.conf
# echo 'path/to/bar' >> /etc/armourd.conf
```

Will put your _foo_ and _bar_ daemons under the watch of armourd.

You can also use file-name globs ('*' wildcard). For example, to watch all the
binaries under /usr/local/bin/:

```
# echo '/usr/local/bin/*' > /etc/armourd.conf
```

System Recovery
---------------

armourd can also be configured to talk to watchdog hardware.  In order to 
make your system fully reliable, you should have a process feeding a watchdog, 
although your mileage may vary.  Linux provides a very simple interface to watchdog timers
(`/dev/watchdog`) that armourd implements as a non-default feature, to
provide an all-round solution. To enable it:
```
./configure --enable-watchdog
```

How it works
------------

Reads kernel events from the proc connector, combined with the use of a socket filter (BPF) to keep things quiet. The proc connector is built on top of the generic connector and that itself is on top of netlink.

For more information, you can read this excellent post from Scott Remnant: http://netsplit.com/2011/02/09/the-proc-connector-and-socket-filters/

Caveats
-------

armourd is Linux-only by design, because it relies upon interfaces such as
[epoll(7)](http://man7.org/linux/man-pages/man7/epoll.7.html) or [netlink(7)](http://man7.org/linux/man-pages/man7/netlink.7.html) 
and [proc(5)](http://man7.org/linux/man-pages/man5/proc.5.html) 
files that aren't available in other unices.
It is more suited to sysvinit-based systems; newer init systems, such as
[upstart](http://upstart.ubuntu.com/) or [systemd](http://www.freedesktop.org/wiki/Software/systemd/), 
service recovery is readily available.

armourd does not start services, i.e. its not an init daemon; on the other hand, 
it runs independently of your init system, and thus is portable across Linux devices.

Forking servers will have their children terminate before restart. Although it is 
designed to recover system daemon programs, it can actually recover any application, 
with the limitation that currently only supports single-instance applications 
unless using the [D-Bus](http://dbus.freedesktop.org/) interface (see below).

*It does not recover applications hanging*. 

Dependencies
------------

The minimum required versions are:

* Linux kernel 2.6.27
* GNU C Library (glibc) 2.8
* GCC 4.1
* Autoconf 2.50
* D-Bus 1.6.x (not required)

D-Bus Interface
---------------

Optionally, you can build the daemon with D-Bus support to commuincate with other 
applications. The D-Bus API exposes runtime information, and a method to watch 
running processes. The introspection XML data is:

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

Examples
--------

Explicitly request to watch a running process, using
DBus, e.g. with `dbus-send` you would:

```sh
$ dbus-send --session --print-reply --type=method_call \
> --dest=com.github.Armourd /com/github/armourd \
> com.github.Armourd.WatchProcess int32:`pidof foo`
```
Or, using the [example](src/dbus/client-example.py) Python script:

```sh
$ ./client-example.py -w`pidof foo`
```


See http://www.linuxfromscratch.org/blfs/view/svn/general/dbus.html for an example of how to build D-Bus

Testsuite
---------

Current version is not production ready, as there aren't any tests.  Unit tests
to exercise the API's and integration tests to check the runtime behaviour are
a must.  This could be added as a configure option (e.g. --enable-tests) then
the user can run the make *check* target

TODO
----

* Testsuite
* Logging
* Configuration file parser needs to be rewritten (it is buggy)
* Enable options in the configuration file and DBus interface to tailor
  specific needs, e.g. return codes for success, recover script, etc.
* Send a D-Bus signal when a watched application crashes/restarts
