armourd
=======

armourd is a Linux daemon for service recovery and is targeted for use in
embedded systems running GNU/Linux.

armourd tracks the creation and termination of processes. When a 'watched'
service crashes, armourd will automatically restart it.  To watch a service,
armourd only needs the file path of the application executable; no PID files, no
scripts.
 
It works out of the box, as it takes roughly a couple of minutes to set it up.
Upon startup armourd interprets its configuration file `/etc/armourd.conf`,
which lists the pathnames of the services that need to be watched.

armourd recovers processes that stopped abnormally, by returning a nonzero exit
code; it does not recover applications hanging.  Forking servers will have their
children terminate before restart.

armourd is Linux-only by design, because it relies upon interfaces such as
`epoll(7)` and `netlink(7)`, and most notably the the use of the `Proc
Connector`. It also makes extensive use of `procfs` files that aren't available
in other unices.

armourd is best suited for systems using sysvinit, as newer init systems, such
as upstart or systemd, service recovery is readily available.

Although armourd is designed to recover system daemon programs, it can actually
recover any application, with the limitation that currently only supports
single-instance applications.

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


Testsuite
---------

Current version is not production ready, as there aren't any tests.  Unit tests
to exercise the API's and integration tests to check the runtime behaviour are a
must.  This could be added as a configure option (e.g. --enable-tests) then the
user can run the make *check* target

Notes
-----

armourd just does individual service recovery; it does not perform system
recovery, i.e. talk to watchdog hardware.  In order to make your system fully
reliable, you should have a process feeding a watchdog, although your mileage
may vary.
Linux provides a very simple interface to watchdog timers (`/dev/watchdog`) that
armourd will implement as a non-default feature, to provide an all-round
solution.

TODO
----

* Testsuite
* Logging
* Configuration file parser needs to be rewritten (it is buggy)
* Enable options in the configuration file to tailor specific needs
* watchdog capabilities
