import alma



def main():
    #kb,res = alma.init(1,'../koca-master/agent.pl','0', 1, 100)
    kb,res = alma.init(1,'demo/comment.pl','0', 1, 1000, ['a', 'b', 'c', 'd', 'e', 'f'], [0.5]*6)

    #alma.set_priorities(kb, ["alpha", "beta", "gamma"], [0.3, 0.5, 1.0])
    # s,l = alma.kbprint(kb)
    # alma.add(kb, "f(alpha).")
    # alma.add(kb, "not(f(beta)).")
    # alma.add(kb, "f(gamma).")
    # alma.add(kb, "if(f(gamma), g).")

    # for line in s.split('\n'):
    #     print(line)
    # print('size: {}'.format(l))
    # alma.step(kb)

    # s,l = alma.kbprint(kb)

    # for line in s.split('\n'):
    #     print(line)
    # print('size: {}'.format(l))

    for i in range(1200):
        step = alma.step(kb)
        print("Step returned: ", step)

        print("i =", i)
        if i %1 == 0:
            s,l = alma.kbprint(kb)
            for line in s.split('\n'):
                print(line)


        for j in range(16):
            alma.add(kb, "empty({}, a{}).".format(i,j))



        # alma.add(kb, "empty({}, south).".format(i))
        # alma.add(kb, "empty({}, east).".format(i))
        # alma.add(kb, "empty({}, west).".format(i))
        # alma.add(kb, "empty({}, anorth).".format(i))
        # alma.add(kb, "empty({}, asouth).".format(i))
        # alma.add(kb, "empty({}, aeast).".format(i))
        # alma.add(kb, "empty({}, awest).".format(i))
        # alma.add(kb, "empty({}, banorth).".format(i))
        # alma.add(kb, "empty({}, basouth).".format(i))
        # alma.add(kb, "empty({}, baeast).".format(i))
        # alma.add(kb, "empty({}, bawest).".format(i))
        # alma.add(kb, "empty({}, canorth).".format(i))
        # alma.add(kb, "empty({}, casouth).".format(i))
        # alma.add(kb, "empty({}, caeast).".format(i))
        # alma.add(kb, "empty({}, cawest).".format(i))


main()
if __name__ == "__main__":
    main()
