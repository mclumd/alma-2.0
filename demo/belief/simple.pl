foo.
fif(foo, bel(bob, quote(p(X)))).
bel(bob, quote(fif(p(X), q(X)))).

fif(now(2), bel(bob, quote(q(X)))).

fif(now(3), bel(bob, quote(not(q(X))))).

bel(bob, quote(bel(carol, quote(foo)))).
bel(bob, quote(bel(carol, quote(fif(foo, bar))))).
