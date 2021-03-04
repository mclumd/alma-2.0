fif(proc(pos_int_spec(quote(foo(`X, `Y))), bound), conclusion(result(X, Y))).
fif(proc(pos_int(quote(foo(`X, `Y))), bound), conclusion(result(X, Y))).
fif(proc(pos_int_gen(quote(foo(`X, `Y))), bound), conclusion(result(X, Y))).

fif(proc(pos_int_spec(quote(foo(X, Y))), bound), conclusion(result(J, K))).
fif(proc(pos_int(quote(foo(X, Y))), bound), conclusion(result(J, K))).
fif(proc(pos_int_gen(quote(foo(X, Y))), bound), conclusion(result(J, K))).

fif(proc(pos_int_spec(quote(foo(a, b))), bound), conclusion(result)).
fif(proc(pos_int(quote(foo(a, b))), bound), conclusion(result)).
fif(proc(pos_int_gen(quote(foo(a, b))), bound), conclusion(result)).

fif(proc(pos_int_spec(quote(foo(`Z, b))), bound), conclusion(result(Z))).
fif(proc(pos_int(quote(foo(`Z, b))), bound), conclusion(result(Z))).
fif(proc(pos_int_gen(quote(foo(`Z, b))), bound), conclusion(result(Z))).

fif(proc(pos_int_spec(quote(foo(`Z, quote(bar(`A))))), bound), conclusion(result(Z))).
fif(proc(pos_int(quote(foo(`Z, quote(bar(`A))))), bound), conclusion(result(Z))).
fif(proc(pos_int_gen(quote(foo(`Z, quote(bar(`A))))), bound), conclusion(result(Z))).
%fif(proc(neg_int_spec(quote()), bound), conclusion()).

foo(const1, Var).
foo(a, b).
foo(C, D).
foo(C, b).
foo(E, f).
