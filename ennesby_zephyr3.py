from waveapi import events, model, ops, robot, simplejson
from google.appengine.api import urlfetch
# appengine/appcfg.py update ennesby
URL = 'http://cesium.is-a-geek.org/zephyr'

def poll(context):
	while True:
		try:
			text = urlfetch.fetch(URL, deadline=10).content
		except urlfetch.DownloadError:
			continue
		break
	context.GetRootWavelet().CreateBlip().GetDocument().SetText(eval(text))

class MyBot(robot.Robot):
	def HandleEvent(self, event, context):
		poll(context)
		#robot.Robot.HandleEvent(self, event, context)

if __name__ == '__main__':
	bot = MyBot('Ennesby',
			image_url='http://ennesby.appspot.com/ennesby.png',
			version='1.34',
			profile_url='http://ennesby.appspot.com/index.html')
	#bot.RegisterHandler(events.WAVELET_BLIP_CREATED, respond)
	#bot.RegisterHandler(events.WAVELET_TITLE_CHANGED, respond)
	#bot.RegisterHandler(events.BLIP_DELETED, respond)
	#bot.RegisterHandler(events.BLIP_SUBMITTED, respond)
	#bot.RegisterHandler(events.DOCUMENT_CHANGED, revert)
	#bot.RegisterHandler(events.FORM_BUTTON_CLICKED, revert)
	bot.RegisterHandler(events.WAVELET_SELF_ADDED, None)
	bot.Run()
