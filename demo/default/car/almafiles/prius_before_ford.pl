obs(quote(rel(gas_tank_car, ford))).
obs(quote(not(rel(fueled, ford)))).

fif(now(5), obs(quote(rel(electric_car, prius)))).
fif(now(5), obs(quote(rel(gas_tank_car, prius)))).
fif(now(5), obs(quote(rel(charged, prius)))).
fif(now(5), obs(quote(not(rel(fueled, prius))))).