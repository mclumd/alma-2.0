% Example 2: a charged prius with an empty gas tank
obs(quote(rel(electric_car, prius1))).
obs(quote(rel(gas_tank_car, prius1))).
obs(quote(rel(charged, prius1))).
obs(quote(not(rel(fueled, prius1)))).

fif(and(rel(gas_tank_car, X), rel(fueled, X)), rel(powered, X)).
fif(and(rel(gas_tank_car, X), not(rel(fueled, X))), not(rel(powered, X))).
fif(and(rel(electric_car, X), rel(charged, X)), rel(powered, X)). 

rel(is_a, gas_tank_car, car).
rel(is_a, electric_car, car).


fif(now(4), obs(quote(rel(electric_car, prius2)))).
fif(now(4), obs(quote(rel(gas_tank_car, prius2)))).
fif(now(4), obs(quote(rel(charged, prius2)))).
fif(now(4), obs(quote(not(rel(fueled, prius2))))).
