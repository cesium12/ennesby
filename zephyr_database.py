#!/usr/bin/python
import getpass, select
import Zephyr as zeph
from zephyr import Zephyr, process
from google.appengine.ext.remote_api import remote_api_stub
APP = 'ennesby'
SUB = (APP, '*', '*')

def loop():
	zobject = zeph.Zephyr()
	zobject.subscribe(SUB)
	while 1:
		select.select([zobject.getfd()], [], [])
		msg = zobject.check()
		while msg:
			Zephyr(**process(msg)).put()
			print '%s: Processed zephyr on -c %s' % (msg['time'], msg['zclass'])
			msg = zobject.check()

if __name__ == '__main__':
	#auth = lambda: (raw_input('Username: '), getpass.getpass('Password: '))
	auth = lambda: ('fuzziusmaximus', getpass.getpass('Password: '))
	remote_api_stub.ConfigureRemoteDatastore(
			APP, '/remote_api', auth, '%s.appspot.com' % APP)
	Zephyr.all(keys_only=True).get() # prompt for user/pwd first
	loop()
