% Current agent knows it's ALMA and has that name
agentname(alma).

% Default assumption for another agent: if this agent isn't near the speaker of an utterance (and aren't the speaker), by default conclude this other agent didn't hear
fif(and(tell(Speaker, Utterance, Confidant),
    and(agent(Agent),
    and(neg_int(quote(agentname(`Agent))),
    neg_int(quote(near(`Agent, `Speaker)))))),
not(heard(Agent, Utterance, Speaker))).

% Agents near the speaker will hear
% Then, confidant must be near to be able to hear
fif(and(tell(Speaker, Utterance, Confidant),
    and(agent(Agent),
    near(Agent, Speaker))),
heard(Agent, Utterance, Speaker)).

% A speaker hears their own utterance
fif(and(tell(Speaker, Utterance, Confidant),
    agent(Speaker)),
heard(Speaker, Utterance, Speaker)).

% An agent that heard from a speaker considers that others near speaker have heard, and believe it itself heard too
common_knowledge(quote(
fif(and(agentname(Self),
    and(heard(Self, Utterance, Speaker),
    near(Agent, Speaker))),
and(heard(Agent, Utterance, Speaker),
bel(Agent, quote(heard(`Self, `Utterance, `Speaker)))))
)).

% An agent that heard from a speaker considers the speaker to have heard, and believe that it itself heard too
common_knowledge(quote(
fif(and(agentname(Self),
    heard(Self, Utterance, Speaker)),
and(heard(Speaker, Utterance, Speaker),
bel(Speaker, quote(heard(`Self, `Utterance, `Speaker)))))
)).

% An agent that heard from a speaker considers by default that others not near speaker have not heard
common_knowledge(quote(
fif(and(agentname(Self),
    and(heard(Self, Utterance, Speaker),
    and(agent(Agent),
    neg_int(quote(near(`Agent, `Speaker)))))),
not(heard(Agent, Utterance, Speaker)))
)).

% Agents are credulous and believe what they hear
% Also a piece of common knowledge among agents
common_knowledge(quote(
fif(and(agentname(Self),
    heard(Self, Utterance, Speaker)),
true(Utterance))
)).

% Make the default assumption that other agents believe they heard what this agent expects that they heard
% Also a piece of common knowledge
common_knowledge(quote(
fif(and(agentname(Self),
    and(heard(Agent, X, Speaker),
    not_equal(quote(agent(`Agent)), quote(agent(`Self))))),
bel(Agent, quote(heard(`Agent, `X, `Speaker))))
)).

% If an agent didn't hear an utterance, expect them to lack that belief in their KB
% Belief formula to be concluded only for agent that's not self
common_knowledge(quote(
fif(and(agentname(Self),
    and(not(heard(Agent, X, Speaker)),
    not_equal(quote(agent(`Agent)), quote(agent(`Self))))),
not(bel(Agent, X)))
)).

% Contradiction response for heard and ~heard: ~heard is a default conclusion, so positive is reinstated
% TODO: Check it really counts the derivation of being a kind of default sort (that is, had neg_int(near())?)
common_knowledge(quote(
fif(contradicting(quote(heard(`Agent, `Belief, `Speaker)), quote(not(heard(`Agent, `Belief, `Speaker))), T),
reinstate(quote(heard(`Agent, `Belief, `Speaker)), T))
)).



% Common knowledge that when decision to give to Agent is made, and believed by Agent, they expect to receive it
common_knowledge(quote(
fif(and(decision(Giver, give(Gift, Agent)),
    agentname(Agent)),
expectation(receive(Agent, Gift)))
)).

% Common knowledge that when an agent has expectation for a gift, they won't be surprised in the future to receive it
common_knowledge(quote(
fif(expectation(receive(Recipient, Gift)),
not(future_surprise(Recipient, gift(Item))))
)).


common_knowledge(quote(
agent(alma)
)).
common_knowledge(quote(
agent(bob)
)).
common_knowledge(quote(
agent(carol)
)).

% Agents believe their own names
common_knowledge(quote(
fif(agent(Agent),
bel(Agent, quote(agentname(`Agent))))
)).

% Being near is symmetric
common_knowledge(quote(
fif(near(A, B), near(B, A))
)).

% ALMA telling Bob of the decision is an event at timestep 0
tell(alma, quote(decision(alma, give(cake, carol))), bob).
