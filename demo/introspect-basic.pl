fif(neg_int_spec(quote(p(X))), conclusion(neg_int_spec_worked(X))).
fif(neg_int(quote(p(X))), conclusion(neg_int_worked(X))).
fif(neg_int_gen(quote(p(X))), conclusion(neg_int_gen_worked(X))).

fif(pos_int_spec(quote(q(Y))), conclusion(pos_int_spec_worked(Y))).
fif(pos_int(quote(q(Y))), conclusion(pos_int_worked(Y))).
fif(pos_int_gen(quote(q(Y))), conclusion(pos_int_gen_worked(Y))).

fif(neg_int_spec(quote(p(`X))), conclusion(neg_int_spec2_worked(X))).
fif(neg_int(quote(p(`X))), conclusion(neg_int2_worked(X))).
fif(neg_int_gen(quote(p(`X))), conclusion(neg_int_gen2_worked(X))).

fif(pos_int_spec(quote(q(`Y))), conclusion(pos_int_spec2_worked(Y))).
fif(pos_int(quote(q(`Y))), conclusion(pos_int2_worked(Y))).
fif(pos_int_gen(quote(q(`Y))), conclusion(pos_int_gen2_worked(Y))).

p(A).
p(aaa).

q(aaa).
q(B).
