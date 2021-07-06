% Example 1: we see a car, and we see it has an empty gas tank
obs(quote(rel(gas_tank_car, ford1))).
obs(quote(not(rel(fueled, ford1)))).

fif(rel(car, X), rel(powered, X)).
rel(is_a, gas_tank_car, car).

fif(and(rel(gas_tank_car, X), not(rel(fueled, X))), not(rel(powered, X))).

