#!/usr/bin/python
import cgitb, select
cgitb.enable()
import Zephyr
from zephyr import process

print """Content-type: text/plain
Cache-Control: no-cache, must-revalidate
Expires: Mon, 26 Jul 1997 05:00:00 GMT
"""

zobject = Zephyr.Zephyr()
zobject.subscribe(('ennesby', '*', '*'))
select.select([zobject.getfd()], [], [])
msg = zobject.check()
while msg:
	process(msg)
	print repr(msg['message'])
	msg = zobject.check()
