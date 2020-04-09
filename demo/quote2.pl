heard(quote(obj_prop_val(Obj, Prop, Val)), T).

fif(heard(X, T),
conclusion(true(X))).

heard(quote(and(foo, if(bar, baz))), 10).
