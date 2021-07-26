rel(is_a, gas_tank_car, car).
rel(is_a, electric_car, car).

fif(rel(car, X), rel(powered, X)).
fif(and(rel(gas_tank_car, X), rel(fueled, X)), rel(powered, X)).
fif(and(rel(gas_tank_car, X), not(rel(fueled, X))), not(rel(powered, X))).
fif(and(rel(electric_car, X), rel(charged, X)), rel(powered, X)).
