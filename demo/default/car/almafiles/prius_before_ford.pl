rel(electric_car, prius).
rel(gas_tank_car, prius).
rel(charged, prius).
not(rel(fueled, prius)).

fif(now(3), rel(gas_tank_car, ford)).
fif(now(3), not(rel(fueled, ford))).

