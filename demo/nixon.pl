% Republicans are typically not pacifists
fif(republican(X),
not(pacifist(X))).

% Quakers are usually pacifist
fif(quaker(X),
pacifist(X)).

% John is republican and quaker
and(republican(john), quaker(john)).
