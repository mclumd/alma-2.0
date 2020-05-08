import alma

a,s = alma.init(1,'../koca-master/agent.pl','0')
s,l = alma.kbprint(a)

for line in s.split('\n'):
    print(line)
print('size: {}'.format(l))
alma.step(a)

s,l = alma.kbprint(a)

for line in s.split('\n'):
    print(line)
print('size: {}'.format(l))
