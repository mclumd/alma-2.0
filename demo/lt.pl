fif(and(now(T), less_than(T, 100)), less(T)).
fif(and(now(T), less_than(300, 100)), 300_less).
fif(and(now(T), less_than(h20, 100)), wat_less).

fif(and(fluent(X, V1),
    and(fluent(X, V2),
    and(acquired(quote(fluent(`X, `V1)), T1),
    and(acquired(quote(fluent(`X, `V2)), T2),
    less_than(T1, T2))))),
distrust(V1)).

fluent(foo, quote(bar)).
bar.
