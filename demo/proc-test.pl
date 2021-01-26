fif(and(query(X),
    and(proc(quote_cons(X, Qx), bound),
    and(proc(acquired(Qx, Y), bound(Qx)), 
    proc(acquired(quote(not(foo(`B))), Z), bound)))),
    conclusion(bar(Y, Z, B))).
not(foo(arg)).
query(hi).
hi.
