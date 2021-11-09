bel(bob, quote(fif(neg_int_spec(quote(p(X))), neg_int_spec_worked(X)))).
bel(bob, quote(fif(neg_int(quote(p(X))), neg_int_worked(X)))).
bel(bob, quote(fif(neg_int_gen(quote(p(X))), neg_int_gen_worked(X)))).

bel(bob, quote(fif(pos_int_spec(quote(q(Y))), pos_int_spec_worked(Y)))).
bel(bob, quote(fif(pos_int(quote(q(Y))), pos_int_worked(Y)))).
bel(bob, quote(fif(pos_int_gen(quote(q(Y))), pos_int_gen_worked(Y)))).

bel(bob, quote(fif(neg_int_spec(quote(p(`X))), neg_int_spec2_worked(X)))).
bel(bob, quote(fif(neg_int(quote(p(`X))), neg_int2_worked(X)))).
bel(bob, quote(fif(neg_int_gen(quote(p(`X))), neg_int_gen2_worked(X)))).

bel(bob, quote(fif(pos_int_spec(quote(q(`Y))), pos_int_spec2_worked(Y)))).
bel(bob, quote(fif(pos_int(quote(q(`Y))), pos_int2_worked(Y)))).
bel(bob, quote(fif(pos_int_gen(quote(q(`Y))), pos_int_gen2_worked(Y)))).

bel(bob, quote(p(A))).
bel(bob, quote(p(aaa))).

bel(bob, quote(q(aaa))).
bel(bob, quote(q(B))).
