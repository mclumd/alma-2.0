fif(and(foo,
    proc(ancestor(bar, foo), bound)),
    conclusion(yes_foo)).
foo.
fif(and(aaaa,
    proc(ancestor(a(X), g(X,Y)), bound)),
    conclusion(a_anc(X,Y))).
bar.
fif(a(Q), conclusion(b(Q))).
if(b(X), c(X)).
if(c(Y), d(Y, hi)).
if(d(X, Y), b(X)).
fif(d(X, Y), conclusion(e(X, Y))).
fif(e(X, Y), conclusion(f(X, Y))).
fif(f(X, Y), conclusion(g(X, Y))).
if(g(A, B), aaaa).
a(arg).
a(asdf).
