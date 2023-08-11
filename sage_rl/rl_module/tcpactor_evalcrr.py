
from sage_rl.offline.offline_agent import make_networks
import sysv_ipc
from envwapper import TCP_dmEnv_Wrapper
from acme.agents.tf import actors
import sys
from ruamel.yaml import YAML
from acme import wrappers
from acme.tf import utils as tf2_utils
from acme import specs
from acme.tf import networks
import sonnet as snt
import tensorflow as tf

import numpy as np
import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'


# use cpu only for evaluation
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
tf.config.set_visible_devices([], 'GPU')

yaml = YAML()


def run_evalcrr(args):

    config_rl_module_path = os.path.dirname(os.path.realpath(__file__))

    with open(os.path.join(config_rl_module_path, "config-rl-eval.yaml"), 'r') as fs:
        config = yaml.load(fs)

    try:
        shrmem_r = sysv_ipc.SharedMemory(args.mem_r)
        shrmem_w = sysv_ipc.SharedMemory(args.mem_w)
    except:
        shrmem_r = None
        shrmem_w = None
        sys.stderr.write("ISSUE WITH SHARED MEMORY ...\n")

        return

    # load env
    env = TCP_dmEnv_Wrapper(config=config['tcpspec'], for_init_only=False, shrmem_r=shrmem_r,
                            shrmem_w=shrmem_w, id=args.id, env_bw=args.bw, num_flows=args.flows)

    env = wrappers.SinglePrecisionWrapper(env)
    environment_spec = specs.make_environment_spec(env)

    policy_network = None

    try:
        nw_type = config['nw_type']
    except:
        nw_type = config['offline_config']['type']

    if nw_type in [1909] or config['offline_config']['use_offline_model']:
        networks_dict = make_networks(environment_spec.actions,
                                      vmin=config['offline_config']['vmin'], vmax=config['offline_config']['vmax'],
                                      num_atoms=config['offline_config']['num_atoms'],
                                      p_lstm_size=config['offline_config']['policy_lstm_size'],
                                      c_lstm_size=config['offline_config']['critic_lstm_size'],
                                      p_enc_size=config['offline_config']['p_enc_size'],
                                      c_enc_size=config['offline_config']['c_enc_size'],
                                      p_mlp_size=config['offline_config']['p_mlp_size'],
                                      c_mlp_size=config['offline_config']['c_mlp_size'],
                                      p_mlp_depth=config['offline_config']['p_mlp_depth'],
                                      c_mlp_depth=config['offline_config']['c_mlp_depth'],
                                      nw_type=config['offline_config']['type']
                                      )
    else:
        raise ValueError("nw_type not supported")

    policy_network = networks_dict['policy']
    tf2_utils.create_variables(network=policy_network,
                               input_spec=[environment_spec.observations])

    

    if config['offline_config']['use_offline_model']:
        if config['offline_config']['load_ckpt_dir'] is not None:
            load_path = config['offline_config']['load_ckpt_dir']
            load_path = os.path.normpath(
                os.path.join(config_rl_module_path, load_path))
            print("Loading model from ckpt_dir: ", load_path)
            assert tf.train.latest_checkpoint(
                load_path) != None, "no checkpoint is loaded, please choose the ckpt directory"
            checkpointer_rl = tf.train.Checkpoint(policy=policy_network)
            checkpointer_rl.restore(
                tf.train.latest_checkpoint(load_path)).expect_partial()
    else:
        raise ValueError("model not supported")

    assert policy_network is not None, "no policy model is chosen, nw_type or set config['offline_config']['use_offline_model']=true in config"

    eval_policy = snt.DeepRNN([
        policy_network,
        networks.StochasticMeanHead(),
        networks.ClipToSpec(environment_spec.actions),
    ])

    actor = actors.RecurrentActor(policy_network=eval_policy)

    iterations = np.int64(0)

    agent_actions_list = []

    

    dummy_observation = np.float32(np.ones((config['tcpspec']['obs_dim'],)))
    actor.select_action(dummy_observation)
    actor._state = actor._network.initial_state(1)
    actor._state = None

    try:
        timestep, _, _ = env.reset0()
        timestep = env.reset()
        actor.observe_first(timestep)

    except Exception as e:
        print(e)
        print("================================ ERROR: TCP env has problem =============================")
        return

    # write 1st action
    a = actor.select_action(timestep.observation)
    env.write_action(a)

    agent_actions_list.append(a)

    def should_not_stop(iterations):
        return True

    episode_return = 0
    actor_steps_taken = 0
    num_episodes = 0
    
    return_report_period = config['tcpactor']['return_report_period']

    while iterations < config['num_training_step']:
        iterations += 1

        next_timestep, _, error_code = env.step0(a, eval_=args.eval)
        next_timestep = env.step(a)

        if error_code == True:

            actor.observe(action=a, next_timestep=next_timestep)

            a1 = actor.select_action(next_timestep.observation)

            env.write_action(a1)

            episode_return += next_timestep.reward
            actor_steps_taken += 1

            

        else:
            sys.stderr.write("Invalid state received...\n")
            env.write_action(a)
            continue

        if iterations % return_report_period == 0:

            episode_return = 0
            num_episodes += 1

        a = a1
