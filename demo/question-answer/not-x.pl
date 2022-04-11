not(rel(foo, x)).

query_truth(quote(rel(foo, x)), 0).
query_belief(quote(rel(foo, x)), 0).

fif(now(3), rel(foo, x)).

fif(now(4), query_truth(quote(rel(foo, x)), 5)).
fif(now(4), query_belief(quote(rel(foo, x)), 5)).

fif(now(5), query_truth(quote(rel(foo, x)), 6)).
fif(now(5), query_belief(quote(rel(foo, x)), 6)).

fif(contradicting(quote(rel(foo, `Arg)), quote(not(rel(foo, `Arg))), T),
reinstate(quote(rel(foo, `Arg)), T)).

fif(now(6), query_truth(quote(rel(foo, x)), 7)).
fif(now(6), query_belief(quote(rel(foo, x)), 7)).
