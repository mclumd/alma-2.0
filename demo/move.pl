fif(and(now(T),
    and(neg_int(quote(seen(`Loc, `Obj))),
    and(empty(L),
    neg_int(quote(doing(`X)))))),
    conclusion(canDo(move(L)))).
