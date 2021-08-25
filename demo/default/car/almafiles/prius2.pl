% A charged prius with an empty gas tank, which also has that electric /\ ~charged --> ~powered
% Leads to additional updating because of multiple possible fifs to both charged and uncharged
obs(quote(rel(electric_car, prius))).
obs(quote(rel(gas_tank_car, prius))).
obs(quote(rel(charged, prius))).
obs(quote(not(rel(fueled, prius)))).

fif(and(rel(electric_car, X), not(rel(charged, X))), not(rel(powered, X))).
