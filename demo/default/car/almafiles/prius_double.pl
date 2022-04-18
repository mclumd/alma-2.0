rel(electric_car, prius1).
rel(gas_tank_car, prius1).
rel(charged, prius1).
not(rel(fueled, prius1)).

fif(now(6), rel(electric_car, prius2)).
fif(now(6), rel(gas_tank_car, prius2)).
fif(now(6), rel(charged, prius2)).
fif(now(6), not(rel(fueled, prius2))).
