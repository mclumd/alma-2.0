obs(quote(rel(electric_car, prius))).
obs(quote(rel(gas_tank_car, prius))).
obs(quote(rel(charged, prius))).
obs(quote(not(rel(fueled, prius)))).

fif(now(5), obs(quote(rel(gas_tank_car, ford)))).
fif(now(5), obs(quote(not(rel(fueled, ford))))).

