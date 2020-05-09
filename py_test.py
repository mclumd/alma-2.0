import alma

a,s = alma.init(1,'../koca-master/agent.pl','0')
s = alma.kbprint(a)

for line in s.split('\n'):
    print(line)
#print('size: {}'.format(l))
alma.step(a)
alma.step(a)
alma.step(a)

s = alma.kbprint(a)

for line in s.split('\n'):
    print(line)
#print('size: {}'.format(l))
