fif(and(bird(X), neg_int(quote(abnormal(X, bird, flies)))), flies(X)).
fif(and(bird(X), not(flies(X))), abnormal(X, bird, flies)).

fif(obs(X), true(X)).
fif(and(contradicting(quote(flies(`X)), quote(not(flies(`X))), T),
    obs(quote(not(flies(`X))))),
reinstate(quote(not(flies(`X))), T)).
fif(and(contradicting(quote(flies(`X)), quote(not(flies(`X))), T),
    obs(quote(flies(`X)))),
reinstate(quote(flies(`X)), T)).

bird(tweety).
bird(clyde).
obs(quote(not(flies(clyde)))).
