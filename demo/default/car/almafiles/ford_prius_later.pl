not(rel(fueled, ford)).
rel(charged, prius).
not(rel(fueled, prius)).

fif(now(0), rel(gas_tank_car, ford)).
fif(now(0), rel(electric_car, prius)).
fif(now(0), rel(gas_tank_car, prius)).

fif(now(11), rel(car, ford2)).
fif(now(11), rel(car, prius2)).
fif(now(11), not(rel(fueled, ford2))).
fif(now(11), rel(charged, prius2)).
fif(now(11), not(rel(fueled, prius2))).

fif(now(12), rel(gas_tank_car, ford2)).
fif(now(12), rel(electric_car, prius2)).
fif(now(12), rel(gas_tank_car, prius2)).
