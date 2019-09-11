import sys

a = []
for b in open(sys.argv[1], 'rb').read():
	a.append('%d' % ord(b))

f = open(sys.argv[2], 'w')
f.write('static unsigned char %s[] = {%s};' % (sys.argv[3], ', '.join(a)))
