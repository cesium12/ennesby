from waveapi import events, blip, robot, appengine_robot_runner
# appengine/appcfg.py update ennesby
from urllib2 import urlopen
from urllib import urlencode
from conf import SEP, NAME, TITLE, PROFILE, ZCOMMIT
import re, sys, traceback

def onJoin(event, wavelet):
	wavelet.title = TITLE
	wavelet.root_blip.append("$ python zephyr_daemon.py '%s' <class...>" % wavelet.wave_id)
	wavelet.root_blip.append("\n\nTo send to a new class and instance, prefix your zephyr with '## <class> ## <instance> ##'.")

def onBlip(event, wavelet):
	try:
		text = event.blip.text.strip()
		match = re.match('##(.*)##(.*)##(.*)$', text)
		if match:
			zclass, instance, message = match.groups()
		else:
			zclass, instance = event.blip.parent_blip.text.strip().split('\n')[0].split(SEP)[:2]
			message = text
		zclass = zclass.strip().replace('/', '')
		instance = instance.strip().replace('/', '')
		signature = wavelet.creator.strip().replace('/', '')
		message = message.strip()
		urlopen(ZCOMMIT + 'class/%s/instance/%s/zsig/%s/' % (zclass, instance, signature), urlencode({ 'payload' : message })).read()
		wavelet.delete(event.blip)
	except Exception, e:
		sys.stderr.write(traceback.format_exc())

def makeBot():
	return robot.Robot(NAME, **PROFILE)

if __name__ == '__main__':
	bot = makeBot()
	bot.register_handler(events.BlipSubmitted, onBlip)
	#bot.register_handler(events.DocumentChanged, respond)
	#bot.register_handler(events.WaveletBlipCreated, onBlip)
	#bot.register_handler(events.WaveletBlipRemoved, respond)
	#bot.register_handler(events.WaveletFetched, revert)
	#bot.register_handler(events.WaveletTitleChanged, None)
	bot.register_handler(events.WaveletSelfAdded, onJoin)
	appengine_robot_runner.run(bot)
