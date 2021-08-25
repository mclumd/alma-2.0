rel(is_a, gas_tank_car, car).
rel(is_a, electric_car, car).

fif(rel(car, X), rel(powered, X)).
fif(and(rel(gas_tank_car, X), rel(fueled, X)), rel(powered, X)).
fif(and(rel(gas_tank_car, X), not(rel(fueled, X))), not(rel(powered, X))).
fif(and(rel(electric_car, X), rel(charged, X)), rel(powered, X)).

% Ontology of mollusk types
rel(is_a, cephalopod, mollusk).
rel(is_a, nautilus, cephalopod).
rel(is_a, naked_nautilus, nautilus).

% Knowledge of shell_bearer status for creature categories
fif(rel(mollusk, X), rel(shell_bearer, X)).
fif(rel(cephalopod, X), not(rel(shell_bearer, X))).
fif(rel(nautilus, X), rel(shell_bearer, X)).
fif(rel(naked_nautilus, X), not(rel(shell_bearer, X))).
