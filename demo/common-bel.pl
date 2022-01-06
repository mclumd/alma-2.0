% Default assumption for another agent: if they aren't near the speaker of an utterance (and aren't the speaker), by default conclude this other agent didn't hear
fif(and(tell(Speaker, Utterance, Confidant),
    and(agent(Agent),
    and(neg_int(quote(near(`Agent, `Speaker))),
    not_equal(quote(agent(`Agent)), quote(agent(`Speaker)))))),
not(heard(Agent, Utterance))).

% Agents near the speaker will hear
% Then, confidant must be near to be able to hear
fif(and(tell(Speaker, Utterance, Confidant),
    and(agent(Agent),
    near(Agent, Speaker))),
heard(Agent, Utterance)).

% A speaker hears their own utterance
fif(and(tell(Speaker, Utterance, Confidant),
    agent(Speaker)),
heard(Speaker, Utterance)).

% Agents are credulous and believe what they hear
% Belief formula to be concluded only for agent other than ALMA
fif(and(heard(Agent, X),
    and(agentname(Self),
    not_equal(quote(agent(`Agent)), quote(agent(Self))))),
bel(Agent, X)).

% If an agent didn't hear an utterance, expect them to lack that belief in their KB
% Belief formula to be concluded only for agent that's not self
fif(and(agentname(Self),
    and(not(heard(Agent, X)),
    not_equal(quote(agent(`Agent)), quote(agent(`Self))))),
not(bel(Agent, X))).

% Being near is reflexive
fif(near(A, B), near(B, A)).

% Contradiction response for heard and ~heard: ~heard is a default conclusion, so positive is reinstated
% TODO: Check it really counts the derivation of being a kind of default sort (that is, had neg_int(near())?)
fif(contradicting(quote(heard(`Agent, `Belief)), quote(not(heard(`Agent, `Belief))), T),
reinstate(quote(heard(`Agent, `Belief)), T)).

% Axioms for the scenario
common_knowledge(quote(agent(alma))).
common_knowledge(quote(agent(bob))).
common_knowledge(quote(agent(carol))).
near(alma, bob).
near(bob, carol).
near(alma, carol). %Temp?
tell(alma, quote(decision(alma, give(cake, carol))), bob).

% Current agent knows it's ALMA
agentname(alma).


% New beliefs

% Other agents will be modeled as believing what ALMA considers common knowledge.
% And, this is also common knowledge
common_knowledge(quote(
fif(and(agent(Agent),
    and(neg_int(quote(agentname(`Agent))),
    common_knowledge(X))),
bel(Agent, quote(common_knowledge(`X))))
)).

fif(and(agent(Agent),
    and(neg_int(quote(agentname(`Agent))),
    common_knowledge(X))),
bel(Agent, X)).

% ALMA agent believes it's common knowledge that common knowledge is true
common_knowledge(quote(
fif(common_knowledge(X),
true(X))
)).

% One duplicated piece of common knowledge to bootstrap: ALMA believes common knowledge
fif(common_knowledge(X),
true(X)).

% Version of above: agents are able to believe common knowledge
% And, this is also common knowledge
common_knowledge(quote(
fif(agent(Agent),
bel(Agent, quote(fif(common_knowledge(X), true(X)))))
)).

% Version of above: agents believe what ALMA considers common knowledge
% Will not infer within agent however
%fif(bel(Agent, quote(common_knowledge(`X))),
%bel(Agent, X)).

% Common knowledge that when decision to give to Agent is made, and believed by Agent, they expect to receive it
common_knowledge(quote(
fif(and(decision(Giver, give(Gift, Agent)),
    agentname(Agent)),
expectation(receive(Agent, Gift)))
)).

common_knowledge(quote(
fif(expectation(receive(Recipient, Gift)),
not(future_surprise(Recipient, gift(Item))))
)).


common_knowledge(quote(
fif(agent(Agent),
bel(Agent, quote(agentname(`Agent))))
)).

%common_knowledge(quote(
%fif(bel(Agent, quote(bel(Agent, `X))),
%bel(Agent, X))
%)).

%fif(and(receive(Agent, gift(Item)), not(bel(Agent, quote(decision(`Agent, gift(`Item)))))), surprise(Agent, gift(Item))).

% Common copies of above

%bel(bob, quote(fif(and(tell(Speaker, Utterance, Confidant),
%    and(agent(Agent),
%    and(neg_int(quote(near(`Agent, `Speaker))),
%    not_equal(quote(agent(`Agent)), quote(agent(`Speaker)))))),
%not(heard(Agent, Utterance))))).

%bel(bob, quote(fif(and(tell(Speaker, Utterance, Confidant),
%    and(agent(Agent),
%    near(Agent, Speaker))),
%heard(Agent, Utterance)))).

%bel(bob, quote(fif(and(tell(Speaker, Utterance, Confidant),
%    agent(Speaker)),
%heard(Speaker, Utterance)))).

%fif(and(heard(Agent, X),
%    not_equal(quote(agent(`Agent)), quote(agent(bob)))),
%bel(Agent, X)).

%bel(bob, quote(fif(and(not(heard(Agent, X)),
%    not_equal(quote(agent(`Agent)), quote(agent(alma)))),
%not(bel(Agent, X))))).

%bel(bob, quote(fif(near(A, B), near(B, A)))).

%bel(bob, quote(fif(contradicting(quote(heard(`Agent, `Belief)), quote(not(heard(`Agent, `Belief))), T),
%reinstate(quote(heard(`Agent, `Belief)), T)))).

%bel(bob, quote(agent(alma))).
%%bel(bob, quote(agent(bob))).
%bel(bob, quote(agent(carol))).
%bel(bob, quote(near(alma, bob))).
%bel(bob, quote(near(bob, carol))).
%bel(bob, quote(tell(alma, quote(decision(alma, gift(cake), carol)), bob))).
