#!/usr/bin/env python

import sys, dbus, getopt

armour_dbus_name = 'com.github.Armourd'
armour_object_path = '/com/github/armourd'

def usage():
    print sys.argv[0], "[--introspect | --watch=PID | --list]"

def main(argv):                         
    global armour_dbus_name, armour_object_path
    bus = dbus.SessionBus()

    try:                                
        opts, args = getopt.getopt(argv, "hiw:l", ["help", "introspect" "watch=", "list"])
    except getopt.GetoptError:
        usage()
        sys.exit(2)     

    try:
        armour_object = bus.get_object(armour_dbus_name, armour_object_path)
    except dbus.DBusException, e:
        print str(e)
        sys.exit(1)

    armour_iface = dbus.Interface(armour_object, 'com.github.Armourd')

    if not opts:
        print 'No options supplied'
        usage()
    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage()
            sys.exit()
        elif opt in ("-i", "--introspect"):
            print armour_object.Introspect(dbus_interface='org.freedesktop.DBus.Introspectable')
        elif opt in ("-w", "--watch"):
            print armour_iface.WatchProcess(int(arg))
        elif opt in ("-l", "--list"):
            proc_dict = armour_iface.ListProcesses()
            for k,v in proc_dict.items():
                print str(k), str(int(v))

if __name__ == '__main__':
    main(sys.argv[1:])

