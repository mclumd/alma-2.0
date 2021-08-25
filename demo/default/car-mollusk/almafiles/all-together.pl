rel(cephalopod, steve).
rel(nautilus, nancy).
rel(naked_nautilus, barry).

if(now(6), obs(quote(rel(cephalopod, sam)))).

obs(quote(rel(gas_tank_car, ford))).
obs(quote(not(rel(fueled, ford)))).

obs(quote(rel(electric_car, prius))).
obs(quote(rel(gas_tank_car, prius))).
obs(quote(rel(charged, prius))).
obs(quote(not(rel(fueled, prius)))).
