% ALMA telling Bob of the decision is an event at timestep 0
tell(alma, quote(decision(alma, give(cake, carol))), bob).

% Current agent knows it's ALMA and has that name
agentname(alma).

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

% An agent considers nearby agents to have heard what it heard
% TODO: should be other agents near speaker; possibly need source of what's heard too
common_knowledge(quote(
fif(and(agentname(Self),
    and(heard(Self, Utterance),
    near(Self, Agent))),
heard(Agent, Utterance))
)).

% Agents are credulous and believe what they hear
% Also a piece of common knowledge among agents
common_knowledge(quote(
fif(and(agentname(Self),
    heard(Self, Utterance)),
true(Utterance))
)).

% Make the default assumption that other agents believe they heard what this agent expects that they heard
% Also a piece of common knowledge
common_knowledge(quote(
fif(and(agentname(Self),
    and(heard(Agent, X),
    not_equal(quote(agent(`Agent)), quote(agent(`Self))))),
bel(Agent, quote(heard(`Agent, `X))))
)).

% If an agent didn't hear an utterance, expect them to lack that belief in their KB
% Belief formula to be concluded only for agent that's not self
common_knowledge(quote(
fif(and(agentname(Self),
    and(not(heard(Agent, X)),
    not_equal(quote(agent(`Agent)), quote(agent(`Self))))),
not(bel(Agent, X)))
)).

% Being near is symmetric
common_knowledge(quote(
fif(near(A, B), near(B, A))
)).

% Contradiction response for heard and ~heard: ~heard is a default conclusion, so positive is reinstated
% TODO: Check it really counts the derivation of being a kind of default sort (that is, had neg_int(near())?)
common_knowledge(quote(
fif(contradicting(quote(heard(`Agent, `Belief)), quote(not(heard(`Agent, `Belief))), T),
reinstate(quote(heard(`Agent, `Belief)), T))
)).

% Axioms for the scenario
common_knowledge(quote(
agent(alma)
)).
common_knowledge(quote(
agent(bob)
)).
common_knowledge(quote(
agent(carol)
)).

common_knowledge(quote(
near(alma, bob)
)).
common_knowledge(quote(
near(bob, carol)
)).
common_knowledge(quote(
near(alma, carol)
)). %Temp?


% Other agents will be modeled as believing what ALMA considers common knowledge.
% And, this is also common knowledge
common_knowledge(quote(
fif(and(agent(Agent),
    and(neg_int(quote(agentname(`Agent))),
    common_knowledge(X))),
bel(Agent, quote(common_knowledge(`X))))
)).

% If this formula replaces the above, models carol correctly but not deeper levels
%fif(and(agent(Agent),
%    and(neg_int(quote(agentname(`Agent))),
%    common_knowledge(X))),
%bel(Agent, X)).

% ALMA agent believes that it's common knowledge that common knowledge is true
%common_knowledge(quote(
%fif(common_knowledge(X),
%true(X))
%)).

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
