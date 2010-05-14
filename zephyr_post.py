from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app
from waveapi import simplejson
import ennesby_zephyr
from conf import FORMAT, KEY, TITLE
import helpers
helpers.opts['format'] = FORMAT

class Handler(webapp.RequestHandler):
	def __init__(self):
		self.robot = ennesby_zephyr.makeBot()
		self.robot.setup_oauth(**KEY)

	def post(self):
		waveid = self.request.POST.get('waveid', None)
		body = self.request.POST.get('body', None)
		if waveid and body:
			#self.response.out.write('Received %s' % body)
			try:
				zephyr = simplejson.loads(body)
				if helpers.valid(zephyr):
					wavelet = self.robot.fetch_wavelet(waveid, 'googlewave.com!conv+root')
					#wavelet.title = TITLE + ' - %s' % zephyr['zclass']
					formatted = helpers.format(zephyr)
					wavelet.reply().append_markup(formatted)
					self.robot.submit(wavelet)
					self.response.out.write(formatted)
			except Exception, e:
				self.response.out.write(' ...but it failed! (%s)' % e)
		else:
			self.response.out.write('Missing waveid or body.')
	
	def get(self):
		self.response.out.write('Please use POST.')

def main():
	application = webapp.WSGIApplication([('/zephyr.*', Handler)], debug=True)
	run_wsgi_app(application)                                    

if __name__ == '__main__':
	main()
