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
true(quote(a), not(b)).
