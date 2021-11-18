bel(bob, quote(fif(a, b))).
bel(bob, quote(fif(b, c))).
bel(bob, quote(a)).
bel(bob, quote(d)).

fif(now(2), bel(bob, quote(fif(ancestor(quote(a), quote(c), 3), anc(a, c))))).
fif(now(2), bel(bob, quote(fif(ancestor(quote(d), quote(c), 3), anc(d, c))))).
fif(now(2), bel(bob, quote(fif(non_ancestor(quote(a), quote(c), 3), non_anc(a, c))))).
fif(now(2), bel(bob, quote(fif(non_ancestor(quote(d), quote(c), 3), non_anc(d, c))))).
