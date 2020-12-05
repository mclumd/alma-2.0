heard(quote(obj_prop_val(Obj, Prop, Val)), T).

fif(heard(X, T),
conclusion(true(X))).

heard(quote(and(foo(X), if(bar(quote(if(const, or(somestuff(a,B,c), otherstuff(d,E,F))))), baz(X)))), 10).
bar(quote(if(const, or(somestuff(a,b,c), otherstuff(d,e,f))))).
bar(quote(if(const, or(somestuff(a,B,c), otherstuff(d,E,F))))).

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
a.
