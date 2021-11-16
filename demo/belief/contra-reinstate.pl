foo.
fif(foo, bel(bob, quote(p(X)))).
bel(bob, quote(fif(p(X), q(X)))).

fif(now(1), not(bel(bob, quote(p(X))))).

fif(now(2), bel(bob, quote(r(X)))).
bel(bob, quote(fif(and(p(X), r(Y)), from_p_and_r))).

fif(now(3), reinstate(quote(bel(bob, quote(p(X)))), 3)).
