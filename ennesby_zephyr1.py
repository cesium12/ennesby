from waveapi import events, model, robot, simplejson
from google.appengine.api import urlfetch
# appengine/appcfg.py update ennesby
URL = 'http://cesium.is-a-geek.org/zephyr'

#from zephyr import Zephyr
#def getZephyr():
#	return Zephyr.blehhh

#import threading
def respond(properties, context):
#	def loop():
		while True:
			try:
				text = urlfetch.fetch(URL, deadline=10).content
			except urlfetch.DownloadError:
				continue
			break
		text = eval(text)
		context.GetRootWavelet().CreateBlip().GetDocument().SetText(text)
#		thread = threading.Thread(target=loop)
#		thread.daemon = True
#		thread.start()
	
#	thread = threading.Thread(target=loop)
#	thread.daemon = True
#	thread.start()
	#context.GetRootWavelet().CreateBlip().GetDocument().SetText('Mango?')
	#context.GetBlipById(properties['blipId']).CreateChild().GetDocument().SetText('Mango?')

if __name__ == '__main__':
	bot = robot.Robot('Ennesby',
			image_url='http://ennesby.appspot.com/ennesby.png',
			version='1.30',
			profile_url='http://ennesby.appspot.com/index.html')
	bot.RegisterHandler(events.WAVELET_SELF_ADDED, respond)
	#bot.RegisterHandler(events.WAVELET_BLIP_CREATED, respond)
	#bot.RegisterHandler(events.WAVELET_TITLE_CHANGED, respond)
	#bot.RegisterHandler(events.BLIP_DELETED, respond)
	#bot.RegisterHandler(events.BLIP_SUBMITTED, respond)
	#bot.RegisterHandler(events.DOCUMENT_CHANGED, revert)
	#bot.RegisterHandler(events.FORM_BUTTON_CLICKED, revert)
	bot.Run()
