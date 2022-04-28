fif(and(rel(electric_car, X), not(rel(charged, X))), not(rel(powered, X))).

rel(gas_tank_car, ford).
not(rel(fueled, ford)).

rel(electric_car, prius).
rel(gas_tank_car, prius).
not(rel(charged, prius)).
not(rel(fueled, prius)).

rel(electric_car, tesla).
not(rel(charged, tesla)).
