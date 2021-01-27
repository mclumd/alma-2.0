fif(and(foo(X), proc(quote_cons(X, Qx), bound(X))),
conclusion(bar(Qx))).

foo(baz(Y)).
foo(baz(Y, quote(asdf(Z)))).
foo(baz(Y, quote(asdf(quote(ghjk(`Z)))))).
foo(not(baz(Y, quote(asdf(quote(ghjk(`Z))))))).
foo(quote(baz(Z))).
