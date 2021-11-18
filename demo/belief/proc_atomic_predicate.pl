bel(bob, quote(a)).
bel(bob, quote(not(a))).
bel(bob, quote(b)).
bel(bob, quote(distrust(quote(b)))).
bel(bob, quote(c)).
bel(bob, quote(update(quote(c), quote(e)))).
bel(bob, quote(true(quote(d)))).
bel(bob, quote(reinstate(quote(not(e)), 0))).
bel(bob, quote(reinstate(quote(e), 0))).

fif(now(1), bel(bob, quote(reinstate(quote(b), 0)))).
