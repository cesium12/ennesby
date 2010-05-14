from waveapi import events, robot, simplejson, appengine_robot_runner
# appengine/appcfg.py update ennesby
from google.appengine.ext import db

class SavedBlip(db.Model):
	blip = db.StringProperty()
	content = db.StringProperty(multiline=True)

def retrieveBlip(blipId):
	return SavedBlip.gql('WHERE blip = :1', blipId).get()

def get(properties, context):
	blipId = properties['blipId']
	if retrieveBlip(blipId):
		revert(properties, context)
		return
	blip = SavedBlip()
	blip.blip = blipId
	blip.content = context.GetBlipById(blipId).GetDocument().GetText()
	blip.put()

def revert(properties, context):
	blipId = properties['blipId']
	doc = context.GetBlipById(blipId).GetDocument()
	blip = retrieveBlip(blipId)
	if blip:
		saved = blip.content
		if doc.GetText() != saved:
			doc.SetText(blip.content)

if __name__ == '__main__':
	bot = robot.Robot('Ennesby',
			image_url='http://ennesby.appspot.com/ennesby.png',
			profile_url='http://ennesby.appspot.com/index.html')
	#bot.register_handler(events.WaveletBlipCreated, get)
	#bot.register_handler(events.WaveletTitleChanged, respond)
	#bot.register_handler(events.BlipDeleted, respond)
	bot.register_handler(events.BlipSubmitted, get)
	bot.register_handler(events.DocumentChanged, revert)
	appengine_robot_runner.run(bot)
