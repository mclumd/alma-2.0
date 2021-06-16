% Example 2: a charged prius with an empty gas tank
obs(quote(rel(electric_car, prius1))).
obs(quote(rel(gas_tank_car, prius1))).
obs(quote(rel(charged, prius1))).
obs(quote(not(rel(fueled, prius1)))).

fif(rel(powered, X), rel(can_drive, X)).
fif(not(rel(powered, X)), not(rel(can_drive, X))).

fif(and(rel(gas_tank_car, X), rel(fueled, X)), rel(powered, X)).
fif(and(rel(gas_tank_car, X), not(rel(fueled, X))), not(rel(powered, X))).
fif(and(rel(electric_car, X), rel(charged, X)), rel(powered, X)). 

rel(is_a, gas_tank_car, car).
rel(is_a, electric_car, car).
