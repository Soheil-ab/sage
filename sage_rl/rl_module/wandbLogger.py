import wandb
from acme.utils.loggers import base
import time
class wandbLogger(base.Logger):
    def __init__(self, job_type, config, group, time_delta=0.0, name=None):


        name = config['environment_name'] + "_" + config['wandb']['proj_name']
        wandb.init(project=name, config=config, resume=False, group=group, job_type=job_type)
        self._time = time.time()
        self._time_delta = time_delta
        if "Actor" not in job_type:
            self._counter = None
        else:
            self._counter = dict(Actor_steps=0)
    def write(self, data):
        now = time.time()
        if (now - self._time) > self._time_delta:
            if self._counter is not None:
                data.update(self._counter)
            wandb.log(data)
            self._time = now

    def actor_steps_counter_update(self, count):
        self._counter['Actor_steps'] = count

