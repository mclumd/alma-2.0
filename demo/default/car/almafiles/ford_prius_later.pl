obs(quote(rel(car, ford))).
obs(quote(rel(car, prius))).

obs(quote(not(rel(fueled, ford)))).
obs(quote(rel(charged, prius))).
obs(quote(not(rel(fueled, prius)))).


fif(now(3), obs(quote(rel(gas_tank_car, ford)))).

fif(now(3), obs(quote(rel(electric_car, prius)))).
fif(now(3), obs(quote(rel(gas_tank_car, prius)))).


fif(now(14), obs(quote(rel(car, ford2)))).
fif(now(14), obs(quote(rel(car, prius2)))).

fif(now(14), obs(quote(not(rel(fueled, ford2))))).
fif(now(14), obs(quote(rel(charged, prius2)))).
fif(now(14), obs(quote(not(rel(fueled, prius2))))).


fif(now(17), obs(quote(rel(gas_tank_car, ford2)))).

fif(now(17), obs(quote(rel(electric_car, prius2)))).
fif(now(17), obs(quote(rel(gas_tank_car, prius2)))).
