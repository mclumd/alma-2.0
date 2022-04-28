% An uncharged prius with gas, which also has that electric /\ ~charged --> ~powered
% Leads to additional updating because of multiple possible fifs to both charged and uncharged
rel(electric_car, prius).
rel(gas_tank_car, prius).
not(rel(charged, prius)).
rel(fueled, prius).

fif(and(rel(electric_car, X), not(rel(charged, X))), not(rel(powered, X))).
