bel(bob, quote(fif(and(foo(X), neg_int(quote(abnormal(`X)))), bar(X)))).
bel(bob, quote(foo(a))).
bel(bob, quote(foo(b))).
bel(bob, quote(fif(foo(b), bar(b)))).

bel(bob, quote(fif(and(bar(X), parents_defaults(quote(bar(`X)), 1)), def_parents(X)))).
bel(bob, quote(fif(and(bar(X), parent_non_default(quote(bar(`X)), 1)), nondef_parent(X)))).
