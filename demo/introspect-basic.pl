fif(proc(neg_int_spec(quote(p(X))), bound), conclusion(neg_int_spec_worked(X))).
fif(proc(neg_int(quote(p(X))), bound), conclusion(neg_int_worked(X))).
fif(proc(neg_int_gen(quote(p(X))), bound), conclusion(neg_int_gen_worked(X))).

fif(proc(pos_int_spec(quote(q(Y))), bound), conclusion(pos_int_spec_worked(Y))).
fif(proc(pos_int(quote(q(Y))), bound), conclusion(pos_int_worked(Y))).
fif(proc(pos_int_gen(quote(q(Y))), bound), conclusion(pos_int_gen_worked(Y))).

fif(proc(neg_int_spec(quote(p(`X))), bound), conclusion(neg_int_spec2_worked(X))).
fif(proc(neg_int(quote(p(`X))), bound), conclusion(neg_int2_worked(X))).
fif(proc(neg_int_gen(quote(p(`X))), bound), conclusion(neg_int_gen2_worked(X))).

fif(proc(pos_int_spec(quote(q(`Y))), bound), conclusion(pos_int_spec2_worked(Y))).
fif(proc(pos_int(quote(q(`Y))), bound), conclusion(pos_int2_worked(Y))).
fif(proc(pos_int_gen(quote(q(`Y))), bound), conclusion(pos_int_gen2_worked(Y))).

p(aaa).
p(A).

q(aaa).
q(B).
