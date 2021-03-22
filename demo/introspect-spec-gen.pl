fif(pos_int_spec(quote(foo(`X, `Y))), conclusion(result(X, Y))).
fif(pos_int(quote(foo(`X, `Y))), conclusion(result(X, Y))).
fif(pos_int_gen(quote(foo(`X, `Y))), conclusion(result(X, Y))).

fif(pos_int_spec(quote(foo(X, Y))), conclusion(result(J, K))).
fif(pos_int(quote(foo(X, Y))), conclusion(result(J, K))).
fif(pos_int_gen(quote(foo(X, Y))), conclusion(result(J, K))).

fif(pos_int_spec(quote(foo(a, b))), conclusion(result)).
fif(pos_int(quote(foo(a, b))), conclusion(result)).
fif(pos_int_gen(quote(foo(a, b))), conclusion(result)).

fif(pos_int_spec(quote(foo(`Z, b))), conclusion(result(Z))).
fif(pos_int(quote(foo(`Z, b))), conclusion(result(Z))).
fif(pos_int_gen(quote(foo(`Z, b))), conclusion(result(Z))).

fif(pos_int_spec(quote(foo(`Z, quote(bar(`A))))), conclusion(result(Z))).
fif(pos_int(quote(foo(`Z, quote(bar(`A))))), conclusion(result(Z))).
fif(pos_int_gen(quote(foo(`Z, quote(bar(`A))))), conclusion(result(Z))).
%fif(neg_int_spec(quote()), conclusion()).

foo(const1, Var).
foo(a, b).
foo(C, D).
foo(C, b).
foo(E, f).
