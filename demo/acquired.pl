fif(and(query(time(X)),
    and(quote_cons(X, Qx, bound(X)),
    acquired(Qx, Y, bound(Qx)))),
answer(Y)).

query(time(now(0))).
if(now(1), query(time(now(2)))).


fif(and(now(T),
    acquired(quote(now(`T)), T)),
now_acquired(T)).
