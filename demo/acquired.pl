fif(and(query(time(X)),
    and(proc(quote_cons(X, Qx), bound(X)),
    proc(acquired(Qx, Y), bound(Qx)))),
conclusion(answer(Y))).
