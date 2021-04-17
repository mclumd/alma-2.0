fif(and(bird(X), neg_int(quote(not(flies(`X))), bound(X))), flies(X)).
bird(tweety).
bird(emu).
not(flies(emu)).

fif(and(tired(tweety), and(pos_int(quote(now(`T))), afternoon(T))),
can_nap(tweety)).
afternoon(5).
