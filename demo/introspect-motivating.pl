fif(and(bird(X), neg_int(quote(not(flies(`X))), bound(X))), conclusion(flies(X))).
bird(tweety).
bird(emu).
not(flies(emu)).

fif(and(tired(tweety), and(pos_int(quote(now(`T))), afternoon(T))),
conclusion(can_nap(tweety))).
afternoon(5).
