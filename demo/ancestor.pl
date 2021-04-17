fif(and(foo,
    ancestor(quote(bar), quote(foo), 8)),
yes_foo).
foo.
fif(and(aaaa,
    ancestor(quote(a(`X)), quote(g(`X,`Y)), 8)),
a_anc(X,Y)).
bar.
fif(a(Q), b(Q)).
if(b(X), c(X)).
if(c(Y), d(Y, hi)).
if(d(X, Y), b(X)).
fif(d(X, Y), e(X, Y)).
fif(e(X, Y), f(X, Y)).
fif(f(X, Y), g(X, Y)).
if(g(A, B), aaaa).
a(arg).
a(asdf).
