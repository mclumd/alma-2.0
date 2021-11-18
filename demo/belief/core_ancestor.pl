bel(bob, quote(fif(a, b))).
bel(bob, quote(fif(b, c))).
bel(bob, quote(a)).

fif(and(now(2), ancestor(quote(bel(bob, quote(a))), quote(bel(bob, quote(c))), 3)), anc(a, c)).
fif(and(now(2), ancestor(quote(bel(bob, quote(d))), quote(bel(bob, quote(c))), 3)), anc(d, c)).

fif(and(now(2), non_ancestor(quote(bel(bob, quote(a))), quote(bel(bob, quote(c))), 3)), non_anc(a, c)).
fif(and(now(2), non_ancestor(quote(bel(bob, quote(d))), quote(bel(bob, quote(c))), 3)), non_anc(d, c)).
