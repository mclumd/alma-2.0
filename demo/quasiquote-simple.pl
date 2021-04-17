fif(and(heard(quote(on_fire(`X))), smoke(X)), on_fire(X)).
if(and(heard(quote(on_fire(`X))), smoke(X)), on_fire(X)).
smoke(site1).

% Only variable
heard(quote(on_fire(Y))).

% Only constant
heard(quote(on_fire(site1))).

% Simple ground function
heard(quote(on_fire(door(site1)))).

% Simple function with variable
heard(quote(on_fire(door(X)))).

% Simple function with quasi-quoted variable
heard(quote(on_fire(door(`X)))).

% Simple ground quotation
heard(quote(on_fire(quote(on_fire(site1))))).
%heard(quote(bel(john, quote(on_fire(site1))))).

% Quotation with variable
heard(quote(on_fire(quote(on_fire(Y))))).
%heard(quote(bel(john, quote(on_fire(Y))))).

% Quotation with singly-quasi-quoted variable
heard(quote(on_fire(quote(on_fire(`Y))))).

% Quotation with doubly quasi-quoted variable
heard(quote(on_fire(quote(on_fire(``Y))))).
smoke(quote(on_fire(Z))).
smoke(quote(on_fire(bar))).
