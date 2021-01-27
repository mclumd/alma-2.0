fif(and(bird(X), proc(neg_int(quote(not(flies(`X)))), bound(X))), conclusion(flies(X))).
bird(tweety).
bird(emu).
not(flies(emu)).

fif(and(tired(tweety), and(proc(pos_int(quote(now(`T))), bound), afternoon(T))),
conclusion(can_nap(tweety))).
afternoon(5).
