fif(and(query(X),
    and(quote_cons(X, Qx),
    and(acquired(Qx, Y, bound(Qx)), 
    acquired(quote(not(foo(`B))), Z)))),
    bar(Y, Z, B)).
not(foo(arg)).
query(hi).
hi.
