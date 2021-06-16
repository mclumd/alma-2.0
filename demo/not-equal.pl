foo(a,b).
foo(c,d).

fif(and(foo(X, Y),
    and(foo(J, K),
    not_equal(quote(foo(`X, `Y)), quote(foo(`J, `K))))),
conc_distinct(X, Y, J, K)).

fif(and(foo(X, Y),
    foo(J, K)),
conc_could_be_same(X, Y, J, K)).
