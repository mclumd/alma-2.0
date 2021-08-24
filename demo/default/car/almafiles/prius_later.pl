obs(quote(rel(car, prius))).
obs(quote(rel(charged, prius))).
obs(quote(not(rel(fueled, prius)))).

fif(now(3), obs(quote(rel(electric_car, prius)))).
fif(now(3), obs(quote(rel(gas_tank_car, prius)))).
