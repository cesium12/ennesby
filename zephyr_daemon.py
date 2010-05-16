#!/usr/bin/python
import getpass, select, sys
from urllib2 import urlopen
from urllib import urlencode
import simplejson
import Zephyr

URL = 'http://ennesby.appspot.com/zephyr'
try:
	name, waveid = sys.argv[:2]
except ValueError:
	sys.exit('Usage: python zephyr_daemon.py <url> <class...>')

def loop():
	zobject = Zephyr.Zephyr()
	for zclass in sys.argv[2:]:
		zobject.subscribe((zclass, '*', '*'))
	zobject.unsubscribe(('message', 'personal', zobject.sender()))
	zobject.unsubscribe(('message', 'urgent', zobject.sender()))
	while 1:
		select.select([zobject.getfd()], [], [])
		msg = zobject.check()
		while msg:
			del msg['zephyr']
			print urlopen(URL, urlencode({ 'waveid' : waveid, 'body' : simplejson.dumps(msg) })).read()
			print '%s: Processed zephyr on -c %s' % (msg['time'], msg['zclass'])
			msg = zobject.check()

if __name__ == '__main__':
	loop()
