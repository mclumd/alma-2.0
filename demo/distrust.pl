foo(c, d).
foo(c, I).
foo(I, J).
foo(I, c).
foo(baz(c), I).
or(foo(c, d), not(bar(const1, f))).
or(foo(c, I), not(bar(const1, J))).

not(bar(const1, const2)).
not(bar(A, B)).

a.
if(a, distrust(quote(foo(`X,`Y)))).
if(a, distrust(quote(foo(`X,Y)))).
if(a, distrust(quote(foo(X,Y)))).

b.
if(b, distrust(quote(not(bar(const1, `Z))))).
if(b, distrust(quote(not(bar(const1, Z))))).

c.
if(c, distrust(quote(or(foo(c, A), not(bar(const1, B)))))).
