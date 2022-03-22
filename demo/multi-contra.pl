p(A,A,A).
p(A,B,B).
p(B,A,B).
p(B,B,A).

not(p(X,X,X)).

fif(now(1), reinstate(quote(not(p(X,X,X))), 1)).
