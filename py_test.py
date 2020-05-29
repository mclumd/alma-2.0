import alma

a,s = alma.init(1,0,'../koca-master/agent.pl','0')
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

s = alma.add(a,"do(move).")
print(s)
s = alma.step(a)
print(s)

s = alma.kbprint(a)

for line in s.split('\n'):
    print(line)

    
s = alma.update(a,"do(move). doing(move).")
print(s)

s = alma.kbprint(a)

for line in s.split('\n'):
    print(line)
