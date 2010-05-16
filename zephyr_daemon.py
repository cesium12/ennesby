#!/usr/bin/python
import getpass, select, sys
from urllib2 import urlopen
from urllib import urlencode
import simplejson
import Zephyr

URL = 'http://ennesby.appspot.com/zephyr'
try:
	waveid = sys.argv[1]
	personal = sys.argv[2] == '-p'
except IndexError:
	sys.exit('Usage: python zephyr_daemon.py <url> [-p] <class...>')
if personal:
	del sys.argv[2]

zobject = Zephyr.Zephyr()
for zclass in sys.argv[2:]:
	zobject.subscribe((zclass, '*', '*'))
if not personal:
	zobject.unsubscribe(('message', 'personal', zobject.sender()))
	zobject.unsubscribe(('message', 'urgent', zobject.sender()))
print 'Subbed to:', zobject.subscribe()
while 1:
	select.select([zobject.getfd()], [], [])
	msg = zobject.check()
	while msg:
		del msg['zephyr']
		print urlopen(URL, urlencode({ 'waveid' : waveid, 'body' : simplejson.dumps(msg) })).read()
		print '%s: Processed zephyr on -c %s' % (msg['time'], msg['zclass'])
		msg = zobject.check()
