%################################################
%#################################################
% VERSION 1.2 D. Josyula, BSU
%#################################################
%#################################################

% Added by Brody to test whether priority based resolution
% can be beneficial.  Basic idea is to do some reasoning
% about the many irrelevant objects seen
if(obj_prop_val(O, location, L), loc(O, L)).

%##################################################### 
% initialization - This part should come from wrapper
%##################################################### 
koca(on).

% To search for an item, specify the type of the item     
% that we are looking for inside search      
% task(search(stoveburner)).
% collaborators(search(stoveburner), on).

% To search for two items of different types, specify the 
%types of the items  that we are looking for inside search      
%task(search(diningtable, chair)).
%collaborators(search(diningtable,chair), on).

% To search for two items of the same type, specify the 
%type of the item and number 2 inside search          
task(searchN(cabinet, 2)).
collaborators(searchN(cabinet,2), on).        
    
%#################################################   
% Task specific knowledge - search
%#################################################

% Purpose/goal of the task    
fif(task(search(Obj)),
conclusion(purpose(search(Obj), find(Obj, location, Val)))).
    
% action/subgoals to accomplish goal
action(find(Obj, location, Val), move(Dir)).

% success conditions definition for goal
success(find(Obj, Prop, Val), obj_prop_val(ObjName, Prop, Val), obj_prop_val(ObjName, type, Obj)).

% If the property value of an object is noted at time T, 
% believe that it continues to hold irrespective of time.  
fif(and(seen(obj_prop_val(Obj, Prop, Val), T),
    proc(neg_int(obj_prop_val(Obj,Prop,Val)), bound)),
conclusion(obj_prop_val(Obj, Prop, Val))).
    
% prerequisites of action 
prereq(move(Dir), empty(Dir)).
    
% optimizing actions
% optimize(move(Dir), promising(Dir)).    
%##########################################################   
% Task specific knowledge - find 2 different types of items
%##########################################################    

% Purpose/goal of the task    
fif(task(search(Obj1,Obj2)),
conclusion(purpose(search(Obj1, Obj2), findObjs(Obj1, Obj2, location)))).     

% action/subgoals to accomplish goal
subgoal(findObjs(Obj1, Obj2, Prop), find(Obj1, Prop, Val1), 1).
subgoal(findObjs(Obj1, Obj2, Prop), find(Obj2, Prop, Val2), 1).

% success conditions definition for goal    
success(findObjs(Obj1, Obj2, Prop), type_obj_prop_val(Obj1, ObjN1,Prop, Val1), type_obj_prop_val(Obj2, ObjN2, Prop, Val2)).

fif(and(goal(findObjs(Obj1, Obj2, Prop)),
    and(obj_prop_val(ObjN1, type, Obj1),
    obj_prop_val(ObjN1, Prop, Val1))),
conclusion(type_obj_prop_val(Obj1, ObjN1, Prop, Val1))).
	
fif(and(goal(findObjs(Obj1, Obj2, Prop)),
    and(obj_prop_val(ObjN2, type, Obj2),
    obj_prop_val(ObjN2, Prop, Val2))),
conclusion(type_obj_prop_val(Obj2, ObjN2, Prop, Val2))).

%##################################################################   
% Task specific knowledge - find 2 different items of the same type
%##################################################################

% Purpose/goal of the task        
fif(task(searchN(Obj, 2)),
conclusion(purpose(searchN(Obj, 2), findN(Obj, location, 2)))).

% action/subgoals to accomplish goal
subgoal(findN(Obj, Prop, Num), find(Obj, Prop, Val), 2).    

% success conditions definition for goal
success(findN(Obj, Prop, Num), num_obj_prop_val(Obj, Prop, ObjN1, ObjN2, Num), obj_prop_val(ObjN2,type,Obj)).
    
% If the property value of an object is noted newly, 
% increment the count of items
fif(and(goal(findN(Obj, Prop,Num)),
    and(obj_prop_val(ObjN, Prop, Val),
    and(obj_prop_val(ObjN, type, Obj),
    proc(neg_int(num_obj_prop_val(Obj, Prop, Ob1, Ob2, SomeNum)), bound)))),    
conclusion(num_obj_prop_val(Obj, Prop, ObjN, none, 1))).

fif(and(goal(findN(Obj, Prop, Num)),
    and(obj_prop_val(Ob2, Prop, Val),
    and(obj_prop_val(Ob2, type, Obj),	    
    and(num_obj_prop_val(Obj, Prop, Ob1, none, 1),    
    proc(neg_int(num_obj_prop_val(Obj,Prop,Ob2,none,1)), bound))))),	        
conclusion(num_obj_prop_val(Obj, Prop, Ob1, Ob2, 2))).
 
%#################################################
% General task knowledge
%#################################################

% note when a task started
fif(and(task(Task),
    and(now(T),
    proc(neg_int(task_startedAt(Task, SomeT)), bound))),
conclusion(task_startedAt(Task, T))).    

%####################################################
% Knowledge of Agency
%####################################################    
% adopt goals towards making a task succeed.    
fif(and(task(Task),
    and(purpose(Task, Goal),
    and(success(Goal, Cond1, Cond2),
        proc(neg_int(Cond1), bound)))),
conclusion(goal(Goal))).

% adopt subgoals without prerequisites to accomplish current goal 
fif(and(goal(Goal),
    and(subgoal(Goal,SG, Num),
        proc(neg_int(prereq(SG, Preq)), bound))),
conclusion(goal(SG))). 

% Attempt actions whose preconditions are met to accomplish current goal     
fif(and(goal(G),
    and(now(T),	    
    and(action(G,A),
    and(prereq(A, Preq),
    and(proc(neg_int(optimize(A,Cond)), bound),	    
    proc(pos_int(Preq), bound)))))),
    conclusion(canDo(A))).

% Attempt optimized actions whose preconditions are met to accomplish current goal     
fif(and(goal(G),
    and(now(T),	    
    and(action(G,A),
    and(prereq(A, Preq),
    and(optimize(A,Cond),
    and(proc(pos_int(Cond), bound),	    
    proc(pos_int(Preq), bound))))))),
conclusion(canDo(A))).

% Attempt actions without prerequisites to accomplish a goal 
fif(and(goal(G),
    and(action(G,A),
    proc(neg_int(prereq(A, Preq)), bound))),
conclusion(canDo(A))).

% If the success condition of a goal is met, note that the goal is satisfied.
fif(and(goal(G),
    and(now(T),	    
    and(success(G, Cond1, Cond2),
    and(proc(pos_int(Cond1), bound),
    and(proc(pos_int(Cond2), bound), 
        proc(neg_int(satisfied(goal(G), Cond1, Cond2, SomeT)), bound)))))),    	 
conclusion(satisfied(goal(G),Cond1,Cond2, T))).

% Once it has been established that the success condition of a task is met,
% note that the task is completed. IF THERE IS MATH, CHECK FOR GREATER TIME 
fif(and(task(Task),
    and(now(X),	    
    and(purpose(Task, Goal),
    and(success(Goal, Cond1, Cond2),
    and(proc(pos_int(Cond1), bound),
    and(proc(pos_int(Cond2), bound), 
        proc(learned(Cond1, T), bound))))))),    	 
conclusion(taskCompletedAt(T))).
    
%---------------------------------------------
% Not sure we need these set of rules for now.    
%---------------------------------------------	
% If the success condition of a goal is met, note that the goal is satisfied.
% fif(and(goal(G),
%    and(success(G, Cond1, Cond2),
%    and(seen(Cond1, T),     
%    seen(Cond2, T)))),
% conclusion(satisfied(goal(G),Cond1,Cond2, T))).


% If the time to accomplish task has passed, 
% no need to hold on to goals to accomplish task.
% fif(and(overtimeAt(Task, O),
%    and(purpose(Task, Goal),
%    and(goal(Goal),
%    proc(neg_int(satisfied(Goal)), bound)))),	    
% conclusion(not(goal(Goal)))).

% Agents can do only one action at a time in this world
% fif(doing(X),
% conclusion(not(canDo(A)))).	    
    
%########################################################
% KoCA rules
%########################################################
% If Koca is on, and collaborators are on for a task, 
% when a success condition for the task is accomplished, 
% broadcast to inform collaborating peers
%MODIFY TO EMIT ONLY obj_prop_val
  
fif(and(koca(on),
    and(task(Task),
    and(collaborators(Task, on),
    and(satisfied(goal(G),Cond1,Cond2,T),
    proc(ancestor(task(Task), satisfied(goal(G),Cond1,Cond2,T)), bound))))),            
conclusion(emit(Cond1, Cond2))).

%and(purpose(Task, G),
    
%If Koca is on, trust heard property values of objects (not just seen) 

fif(and(koca(on),
    heard(type_obj_prop_val(Type, Obj, Prop, Val), T)),
conclusion(type_obj_prop_val(Type, Obj, Prop, Val))).
    
fif(and(koca(on),
    heard(obj_prop_val(Obj, Prop, Val), T)),
conclusion(obj_prop_val(Obj, Prop, Val))).

fif(and(koca(on),
    heard(num_obj_prop_val(Obj, Prop, Ob1, Ob2, Val), T)),
conclusion(num_obj_prop_val(Obj, Prop, Ob1, Ob2, Val))).
     
%ALMA CHANGE NEEDED FOR THE GENERAL RULE BELOW TO WORK
     
%fif(and(koca(on),
%        heard(Fact),
%conclusion(Fact).

