bel(bob, quote(x)).
bel(bob, quote(not(x))).

fif(now(1), bel(bob, quote(fif(pos_int_past(quote(x), Start, End), pos_int_past_success)))).
fif(now(1), bel(bob, quote(fif(neg_int_past(quote(y), Start, End), neg_int_past_success)))).
