fif(and(foo(X), quote_cons(X, Qx, bound(X))),
bar(Qx)).

foo(baz(Y)).
foo(baz(Y, quote(asdf(Z)))).
foo(baz(Y, quote(asdf(quote(ghjk(`Z)))))).
foo(not(baz(Y, quote(asdf(quote(ghjk(`Z))))))).
foo(quote(baz(Z))).
