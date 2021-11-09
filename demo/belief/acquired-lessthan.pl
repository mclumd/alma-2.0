bel(bob, quote(fif(less_than(100, 300), 100_less))).
bel(bob, quote(fif(less_than(h20, 100), wat_less))).

bel(bob, quote(fif(not_equal(100, 300), unequal))).
bel(bob, quote(fif(not_equal(quote(p(`X)), quote(p(`X))), unequal(p(`X))))).

bel(bob, quote(fif(and(fluent(X, V1),
    and(fluent(X, V2),
    and(acquired(quote(fluent(`X, `V1)), T1),
    and(acquired(quote(fluent(`X, `V2)), T2),
    less_than(T1, T2))))),
distrust(V1)))).

bel(bob, quote(fluent(foo, quote(bar)))).
bel(bob, quote(bar)).

% T=2 or onward: add bel(bob, quote(fluent(foo, quote(baz)))).
