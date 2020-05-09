fif(and(now(T), proc(less_than(T, 100), bound(T))), conclusion(less(T))).
fif(and(now(T), proc(less_than(300, 100), bound)), conclusion(300_less)).
fif(and(now(T), proc(less_than(h20, 100), bound)), conclusion(wat_less)).

fif(and(fluent(X, V1),
    and(fluent(X, V2),
    and(proc(learned(fluent(X, V1), T1), bound),
    and(proc(learned(fluent(X, V2), T2), bound),
    proc(less_than(T1, T2), bound))))),
conclusion(distrust(V1))).

fluent(foo, bar).
bar.
