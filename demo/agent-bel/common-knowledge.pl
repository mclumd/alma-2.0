% Meta-knowledge about common knowledge

% Other agents will be modeled as believing what ALMA considers common knowledge.
% And, this is also common knowledge
common_knowledge(quote(
fif(and(agent(Agent),
    and(neg_int(quote(agentname(`Agent))),
    common_knowledge(X))),
bel(Agent, quote(common_knowledge(`X))))
)).

% If this formula replaces the above, models one level correctly but not deeper levels
%fif(and(agent(Agent),
%    and(neg_int(quote(agentname(`Agent))),
%    common_knowledge(X))),
%bel(Agent, X)).

% One duplicated piece of common knowledge to bootstrap: ALMA believes common knowledge
fif(common_knowledge(X),
true(X)).

% Version of above: agents are able to believe common knowledge
% And, this is also common knowledge
common_knowledge(quote(
fif(and(agent(Agent),
    neg_int(quote(agentname(`Agent)))),
bel(Agent, quote(fif(common_knowledge(X), true(X)))))
)).


% Meta-knowledge about pairwise common knowledge

% If there is a pair knowledge formula with another agent, that agent believes the formula as pair knowledge
common_knowledge(quote(
fif(and(pair_knowledge(Agent_a, Agent_b, X),
    and(agentname(Self),
    not_equal(quote(agent(`Agent_b)), quote(agent(`Self))))),
bel(Agent_b, quote(pair_knowledge(`Agent_a, `Agent_b, `X))))
)).
common_knowledge(quote(
fif(and(pair_knowledge(Agent_a, Agent_b, X),
    and(agentname(Self),
    not_equal(quote(agent(`Agent_a)), quote(agent(`Self))))),
bel(Agent_a, quote(pair_knowledge(`Agent_a, `Agent_b, `X))))
)).

% Agents believe the formula that's pair knowledge they have a part in
common_knowledge(quote(
fif(and(agentname(Self),
    pair_knowledge(Self, Agent_b, X)),
true(X))
)).
common_knowledge(quote(
fif(and(agentname(Self),
    pair_knowledge(Agent_a, Self, X)),
true(X))
)).
