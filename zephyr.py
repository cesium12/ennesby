#!/usr/bin/python
import datetime
from google.appengine.ext import db

fields = {
		#'port', 'from_host', 'recipient',
		'zclass' : db.StringProperty(),
		'instance' : db.StringProperty(),
		'sender' : db.StringProperty(),
		'opcode' : db.StringProperty(),
		'signature' : db.TextProperty(),
		'message' : db.TextProperty(),
		'time' : db.DateTimeProperty()
		}

Zephyr = type('Zephyr', (db.Model,), fields)

def process(msg):
	msg['time'] = datetime.datetime.now()
	return msg
