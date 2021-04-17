heard(quote(obj_prop_val(Obj, Prop, Val)), T).

fif(heard(X, T),
true(X)).

heard(quote(and(foo(X), if(bar(quote(if(const, or(somestuff(a,B,c), otherstuff(d,E,F))))), baz(X)))), 10).
bar(quote(if(const, or(somestuff(a,B,c), otherstuff(d,E,B))))).
bar(quote(if(const, or(somestuff(a,B,c), otherstuff(d,E,F))))).

if(bar(quote(if(const, or(somestuff(a,X,c), otherstuff(d,Y,Z))))),
   stuffy_conc).

true(bar).
true(quote(test)).
true(quote(test)).
true(quote(test)).

true(quote(foo(A)), not(b)).
true(quote(foo(X)), not(X)).
true(quote(foo(X)), not(Y)).

foo(quote(or(a(X), a(quote(b(quote(c(X)))))))).
foo(quote(or(a(Z), a(quote(b(quote(c(Y)))))))).

test(quote(or(foo(X), bar(quote(secondlevel(X, quote(thirdlevel(X))))))),
     quote(or(foo(X), bar(quote(secondlevel(X, quote(thirdlevel(X)))))))).

if(a,
   test2(quote(or(foo(X), bar(quote(secondlevel(X, quote(thirdlevel(X))))))),
         quote(or(foo(X), bar(quote(secondlevel(X, quote(thirdlevel(X))))))))).
if(a,
   test2(quote(or(foo(L), bar(quote(secondlevel(M, quote(thirdlevel(N))))))),
         quote(or(foo(X), bar(quote(secondlevel(Y, quote(thirdlevel(Z))))))))).
if(a,
   test2(quote(or(foo(L), bar(quote(secondlevel(M, quote(thirdlevel(M))))))),
         quote(or(foo(X), bar(quote(secondlevel(Y, quote(thirdlevel(Z))))))))).

a.

if(test2(quote(or(foo(A), bar(quote(secondlevel(B, quote(thirdlevel(C))))))),
         quote(or(foo(D), bar(quote(secondlevel(E, quote(thirdlevel(F)))))))),
  conc(A, B, C, D, E, F)).
