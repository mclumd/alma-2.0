fif(proc(neg_int(quote(p(X))), bound), conclusion(neg_int_worked(X))).
fif(proc(pos_int(quote(q(Y))), bound), conclusion(pos_int_worked(Y))).

fif(proc(neg_int(quote(p(`X))), bound), conclusion(neg_int2_worked(X))).
fif(proc(pos_int(quote(q(`Y))), bound), conclusion(pos_int2_worked(Y))).

p(aaa).
q(aaa).
