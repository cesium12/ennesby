from waveapi import events, model, robot, simplejson
# appengine/appcfg.py update ennesby
import urllib, urllib2, htmllib

def unescape(s):
	p = htmllib.HTMLParser(None)
	p.save_bgn()
	p.feed(s)
	return p.save_end()

def translate(text, src, to):
	args = urllib.urlencode({ 'langpair' : '%s|%s' % (src, to), 'v' : '1.0', 'q' : text })
	url = 'http://ajax.googleapis.com/ajax/services/language/translate'
	resp = simplejson.load(urllib2.urlopen(url, data=args))
	return resp['responseData']['translatedText'].encode('utf8')

def respond(properties, context):
	langs = [ 'en', 'ja' ]
	doc = context.GetBlipById(properties['blipId']).GetDocument()
	text = doc.GetText()
	history = set()
	for i in range(20):
		text = translate(text, *langs)
		try:
			doc.AppendText('\n%s' % unescape(text))
		except UnicodeDecodeError:
			pass
		if text in history:
			break
		history.add(text)
		langs.reverse()

if __name__ == '__main__':
	bot = robot.Robot('Ennesby',
			image_url='http://ennesby.appspot.com/ennesby.png',
			version='1.20',
			profile_url='http://ennesby.appspot.com/index.html')
	#bot.RegisterHandler(events.WAVELET_SELF_ADDED, respond)
	#bot.RegisterHandler(events.WAVELET_BLIP_CREATED, respond)
	bot.RegisterHandler(events.BLIP_SUBMITTED, respond)
	bot.Run()
