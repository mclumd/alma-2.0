fif(and(query(X), 
    and(proc(acquired(X, Y), bound(X)), 
    proc(acquired(not(foo(B)), Z), bound))),
    conclusion(bar(Y, Z, B))).
not(foo(arg)).
query(hi).
hi.
