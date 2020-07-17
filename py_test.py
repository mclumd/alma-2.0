import alma



def main():
    #kb,res = alma.init(1,'../koca-master/agent.pl','0', 1, 100)
    kb,res = alma.init(1,'demo/comment.pl','0', 1, 100, ["alpha", "beta", "gamma"], [0.3, 0.5, 1.0])
    #alma.set_priorities(kb, ["alpha", "beta", "gamma"], [0.3, 0.5, 1.0])
    s,l = alma.kbprint(kb)
    alma.add(kb, "f(alpha).")
    alma.add(kb, "not(f(beta)).")
    alma.add(kb, "f(gamma).")
    alma.add(kb, "if(f(gamma), g).")

    for line in s.split('\n'):
        print(line)
    print('size: {}'.format(l))
    alma.step(kb)

    s,l = alma.kbprint(kb)

    for line in s.split('\n'):
        print(line)
    print('size: {}'.format(l))

    for i in range(120):
        alma.step(kb)

        s,l = alma.kbprint(kb)

        for line in s.split('\n'):
            print(line)
        print('i: {} \tsize: {}'.format(i, l))
        alma.add(kb, "empty({}, north).".format(i))
        alma.add(kb, "empty({}, south).".format(i))
        alma.add(kb, "empty({}, east).".format(i))
        alma.add(kb, "empty({}, west).".format(i))
        alma.add(kb, "empty({}, anorth).".format(i))
        alma.add(kb, "empty({}, asouth).".format(i))
        alma.add(kb, "empty({}, aeast).".format(i))
        alma.add(kb, "empty({}, awest).".format(i))


main()
if __name__ == "__main__":
    main()
