empty(left).
empty(right).
empty(up).
empty(down).
fif(and(now(T),
    and(proc(neg_int(seen(Loc, Obj)), bound(T)),
    and(empty(L),
    proc(neg_int(doing(X)), bound)))),
    conclusion(canDo(move(L)))).
if(doing(X), done(X)).
