rel(mollusk, steve).
if(now(2), rel(cephalopod, steve)).
if(now(7), rel(nautilus, steve)).
if(now(12), rel(naked_nautilus, steve)).

obs(quote(rel(gas_tank_car, ford))).
obs(quote(not(rel(fueled, ford)))).

fif(now(5), obs(quote(rel(electric_car, prius)))).
fif(now(5), obs(quote(rel(gas_tank_car, prius)))).
fif(now(5), obs(quote(rel(charged, prius)))).
fif(now(5), obs(quote(not(rel(fueled, prius))))).
