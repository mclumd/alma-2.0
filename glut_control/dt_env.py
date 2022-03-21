import alma
import sys
import math

sys.path.append("/home/justin/alma-2.0/alma_python")
import alma_utils

class alma_env:
    def __init__(self, kb, reward_fn, max_steps=math.inf):
        self.alma_client, _ = alma.init(1,kb, '0', 1, 1000, [], [])
        self.reward_fn = reward_fn
        self.kb = kb
        self.done=False
        self.num_steps = 0
        self.max_steps = max_steps


    def reset(self):
        self.halt()
        self.alma_client, _ = alma.init(1,self.kb, '0', 1, 1000, [], [])
        self.done=False
        self.num_steps = 0 

    def step(self, action):
        """
        return state, reward, done, info (==None)
        with state == KB
        """
        self.apply_action(action)
        state = alma_utils.current_kb_text(self.alma_client)
        self.num_steps += 1
        self.current_kb = state
        done = self.done()
        reward = self.reward_fn(state)
        return state, reward, done, None
        
            
    
    def done(self):
        if (self.num_steps >= self.max_steps) or any(['done' in x for x in self.current_kb]):
            return True
        else:
            return False
        
    def apply_action(self, action_num):
        # For now, just ignore the action number
        # with the assumption that we're just
        # going to pull the top action from the
        # priority queue.
        prb = alma.prebuf(self.alma_client)
        res_tasks = prb[0]
        if False:
            # Stuff to set priorities before proceeding.
            priorities = np.random.uniform(size = len(res_task_input))
            alma.set_priors_prb(alma_inst, priorities.flatten().tolist())
        alma.prb_to_res_task(alma_client, 1.0)


        

        alma.astep(alma_inst)



    
    def add_obs(self, category):
        alma.obs(self.alma_client, category + '.')

    def add_category(self, category):
        alma.add(self.alma_client, category + '.')
        
    def cstep(self):
        alma_utils.classical_step(self.alma_client)

    def astep(self):
        alma_utils.astep(self.alma_client)


    def halt(self):
        alma.halt(self.alma_client)

# STATIC METHODS
# REWARD FUNCTIONS:  these all take a list of strings
#                    (KB) as input and return a reward
def reward_right1(kb):
    return sum([s.count("right") for s in kb if "(a)" in s])

def reward_f1(kb):
    return sum([s.count("f") for s in kb if "(a)" in s])
