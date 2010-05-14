from waveapi import events, model, ops, robot, simplejson
from google.appengine.api import urlfetch
from google.appengine.ext import db
# appengine/appcfg.py update ennesby
URL = 'http://cesium.is-a-geek.org/zephyr'
import pickle, sys

class SavedWavelet(db.Model):
	wavelet = db.BlobProperty()

def poll(context):
	context.GetRootWavelet().CreateBlip().GetDocument().SetText('mangoes!!')
	'''
	while True:
		try:
			text = urlfetch.fetch(URL, deadline=10).content
		except urlfetch.DownloadError:
			continue
		break
	context.GetRootWavelet().CreateBlip().GetDocument().SetText(eval(text))
	'''

def join(properties, context):
	SavedWavelet(wavelet=pickle.dumps(context)).put()
	poll(context)

if __name__ == '__main__':
	saved = SavedWavelet.all().get()
	if saved is not None:
		poll(pickle.loads(saved.wavelet))
	bot = robot.Robot('Ennesby',
			image_url='http://ennesby.appspot.com/ennesby.png',
			version='1.32',
			profile_url='http://ennesby.appspot.com/index.html')
	#bot.RegisterHandler(events.WAVELET_BLIP_CREATED, respond)
	#bot.RegisterHandler(events.WAVELET_TITLE_CHANGED, respond)
	#bot.RegisterHandler(events.BLIP_DELETED, respond)
	#bot.RegisterHandler(events.BLIP_SUBMITTED, respond)
	#bot.RegisterHandler(events.DOCUMENT_CHANGED, revert)
	#bot.RegisterHandler(events.FORM_BUTTON_CLICKED, revert)
	bot.RegisterHandler(events.WAVELET_SELF_ADDED, join)
	bot.Run()
