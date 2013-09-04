#!/usr/bin/env python

import sys, dbus, getopt

armour_dbus_name = 'com.github.Armourd'
armour_object_path = '/com/github/armourd'
watch_ret = {0 : 'Success', 1 : 'Already watching', 2 : 'No Memory'}

def usage():
    print "Usage:", sys.argv[0], "[--introspect | --watch=PID | --list]"

def main(argv):                         
    global armour_dbus_name, armour_object_path
    global watch_ret
    bus = dbus.SessionBus()

    try:                                
        opts, args = getopt.getopt(argv, "hiw:l", ["help", "introspect", "watch=", "list"])
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
            ret_code = armour_iface.WatchProcess(int(arg))
            print watch_ret[ret_code]
        elif opt in ("-l", "--list"):
            proc_dict = armour_iface.ListProcesses()
            for k,v in proc_dict.items():
                print k, v

if __name__ == '__main__':
    main(sys.argv[1:])

