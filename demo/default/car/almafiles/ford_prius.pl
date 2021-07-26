% Example 1: we see a car, and we see it has an empty gas tank
obs(quote(rel(gas_tank_car, ford1))).
obs(quote(not(rel(fueled, ford1)))).

% Example 2: a charged prius with an empty gas tank
obs(quote(rel(electric_car, prius1))).
obs(quote(rel(gas_tank_car, prius1))).
obs(quote(rel(charged, prius1))).
obs(quote(not(rel(fueled, prius1)))).
