#!/bin/sh
case `uname` in
Darwin*)
	dns-sd -B _services._dns-sd._udp.
	;;
Linux*)
	avahi-browse _services._dns-sd._udp
	;;
*)
	echo "$0: unknown operating system:" `uname`
	exit 1
esac
