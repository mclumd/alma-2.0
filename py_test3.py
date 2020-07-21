import alma

a,s = alma.init(1,0,'../koca-master/agent.pl','0')

d = alma.kb_to_pyobject(a)

s = alma.kbprint(a)
s = s.split('\n')
print_len = len(s)
list_len = len(d)
print('print len: {}'.format(len(s)))
print('list len: {}'.format(len(d)))

max_len = max(print_len,list_len)

for i in range(max_len):
    if i < print_len:
        print(s[i])
    if i < list_len:
        print(d[i])

    print('')
