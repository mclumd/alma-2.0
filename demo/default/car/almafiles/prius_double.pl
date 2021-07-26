% Example 2: a charged prius with an empty gas tank
obs(quote(rel(electric_car, prius1))).
obs(quote(rel(gas_tank_car, prius1))).
obs(quote(rel(charged, prius1))).
obs(quote(not(rel(fueled, prius1)))).

fif(now(4), obs(quote(rel(electric_car, prius2)))).
fif(now(4), obs(quote(rel(gas_tank_car, prius2)))).
fif(now(4), obs(quote(rel(charged, prius2)))).
fif(now(4), obs(quote(not(rel(fueled, prius2))))).
