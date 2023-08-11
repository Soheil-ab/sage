

import sonnet as snt
from sage_rl.offline.unplug_networks import ControlNetwork
from acme.tf import networks as acme_networks
from typing import Dict, Optional, Tuple, Set, Sequence
from absl import flags
from absl import app

from sage_rl.offline.gen_offline_data import build_dataset
from acme.tf import utils as tf2_utils
import copy
from sage_rl.rl_module.env_utils import make_env


from sage_rl.rl_module.log_utils import make_logger


from acme import specs


import functools

import numpy as np
import os
import pandas as pd
import tensorflow as tf

from os import listdir
import argparse

from ruamel.yaml import YAML

yaml = YAML()


# Set the GPU memory properly
os.environ["CUDA_VISIBLE_DEVICES"] = "0"
physical_devices = tf.config.list_physical_devices('GPU')
try:
    tf.config.experimental.set_memory_growth(physical_devices[0], True)
except:
    # Invalid device or cannot modify virtual devices once initialized.
    pass


def make_networks(
    action_spec: specs.BoundedArray,
    act_fn: str = "tanh",
    policy_lstm_sizes: Sequence[int] = None,
    critic_lstm_sizes: Sequence[int] = None,
    num_components: int = 5,
    vmin: float = 0.,
    vmax: float = 100.,
    num_atoms: int = 51,
    nw_type: int = 1909,

    p_enc_size: int = 256,
    p_mlp_size: int = 256,
    p_mlp_depth: int = 2,
    c_enc_size: int = 256,
    c_mlp_size: int = 256,
    c_mlp_depth: int = 2,
    p_lstm_size: int = 256,
    c_lstm_size: int = 256,
    lstm_depth: int = 1,
):

    action_size = np.prod(action_spec.shape, dtype=int)
    actor_head = acme_networks.MultivariateGaussianMixture(
        num_components=num_components, num_dimensions=action_size)

    if nw_type == 1909:
        if act_fn == "tanh":
            act = tf.nn.tanh
        elif act_fn == "leaky":
            act = tf.nn.leaky_relu
        else:
            act = tf.nn.relu

        policy_lstm_sizes = [p_lstm_size for i in range(lstm_depth)]
        critic_lstm_sizes = [c_lstm_size for i in range(lstm_depth)]

        actor_neck = acme_networks.LayerNormAndResidualMLP(
            hidden_size=p_mlp_size, num_blocks=p_mlp_depth)
        actor_encoder = ControlNetwork(
            proprio_encoder_size=p_enc_size, activation=act)
        actor_encoder2 = ControlNetwork(
            proprio_encoder_size=p_enc_size, activation=act)
        policy_lstms = [snt.GRU(s) for s in policy_lstm_sizes]
        policy_network = snt.DeepRNN(
            [actor_encoder] + policy_lstms + [actor_encoder2] + [actor_neck] + [actor_head])

        critic_encoder = ControlNetwork(
            proprio_encoder_size=c_enc_size, activation=act)
        critic_encoder2 = ControlNetwork(
            proprio_encoder_size=c_enc_size, activation=act)
        critic_neck = acme_networks.LayerNormAndResidualMLP(
            hidden_size=c_mlp_size, num_blocks=c_mlp_depth)
        distributional_head = acme_networks.DiscreteValuedHead(
            vmin=vmin, vmax=vmax, num_atoms=num_atoms)
        critic_lstms = [snt.GRU(s) for s in critic_lstm_sizes]
        critic_network = acme_networks.CriticDeepRNN(
            [critic_encoder] + critic_lstms + [critic_encoder2] + [critic_neck] + [distributional_head])

    else:
        raise NotImplementedError

    return {
        'policy': policy_network,
        'critic': critic_network,
    }


def run_crr():

    parser = argparse.ArgumentParser()
    parser.add_argument('--policy_lr', type=float)
    parser.add_argument('--critic_lr', type=float)
    parser.add_argument('--dqda_norm', type=float)
    parser.add_argument('--cold_start', type=int)
    parser.add_argument('--sequence_length', type=int)
    parser.add_argument('--batch_size', type=int)
    parser.add_argument('--replay_period', type=int)
    parser.add_argument('--vmin', type=float)
    parser.add_argument('--vmax', type=float)
    parser.add_argument('--num_atoms', type=int)
    parser.add_argument('--dataset_name', type=str)
    parser.add_argument('--add_uid', action='store_true')
    parser.add_argument('--config', type=str, default=None,
                        help="if None, use configs/config-rl, or use specifed in not None")

    args = parser.parse_args()

    if args.config is None:

        config_rl_module_path = os.getcwd()
        with open(os.path.join(config_rl_module_path, "configs/config-rl-offline.yaml"), 'r') as fs:
            config = yaml.load(fs)
    else:
        with open(args.config, 'r') as fs:
            config = yaml.load(fs)

    try:
        a = config['offline_config']['shuffle_buffer_size']
    except:
        config['offline_config']['shuffle_buffer_size'] = 1000000
        config['offline_config'].update({"shuffle_buffer_size": 1000000})

    try:
        a = config['offline_config']['block_length']
    except:
        config['offline_config']['block_length'] = 1
        config['offline_config'].update({"block_length": 1})

    try:
        a = config['offline_config']['lstm_depth']
    except:
        config['offline_config']['lstm_depth'] = 1
        config['offline_config'].update({"lstm_depth": 1})

    try:
        a = config['offline_config']['act_fn']
    except:
        config['offline_config']['act_fn'] = "tanh"
        config['offline_config'].update({"act_fn": "tanh"})

    try:
        a = config['offline_config']['should_shuffle']
    except:
        config['offline_config']['should_shuffle'] = True
        config['offline_config'].update({"should_shuffle": True})

    try:
        a = config['offline_config']['interleave_type']
    except:
        config['offline_config']['interleave_type'] = "none"
        config['offline_config'].update({"interleave_type": "none"})

    for arg in vars(args):
        value = getattr(args, arg)
        if value is not None:
            print(arg)
            config.update({arg: value})
    if config['offline_config']['add_uid']:
        from acme.utils import paths
        config['offline_config'].update({"uid": paths.get_unique_id()[0]})

    environment_name = config['environment_name']
    environment = make_env(environment_name, config)
    environment_spec = specs.make_environment_spec(environment)

    print("envspec:", environment_spec.observations)
    print("actionspec:", environment_spec.actions)

    # create policy and critic networks
    if environment_name == 'tcp':

        nets = make_networks(environment_spec.actions,
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
                             nw_type=config['offline_config']['type'],
                             lstm_depth=config['offline_config']['lstm_depth'],
                             act_fn=config['offline_config']['act_fn']
                             )
        policy_network, critic_network = nets['policy'], nets['critic']

        logger = make_logger('models/crr_learner', 'learner',
                             time_delta=5, use_tb=config['use_learner_tb'])

    target_policy_network = copy.deepcopy(policy_network)
    target_critic_network = copy.deepcopy(critic_network)
    tf2_utils.create_variables(network=policy_network,
                               input_spec=[environment_spec.observations])
    tf2_utils.create_variables(network=critic_network,
                               input_spec=[environment_spec.observations, environment_spec.actions])
    tf2_utils.create_variables(network=target_policy_network,
                               input_spec=[environment_spec.observations])
    tf2_utils.create_variables(network=target_critic_network,
                               input_spec=[environment_spec.observations, environment_spec.actions])

    dataset = build_dataset(args.dataset_name, seq_len=config['offline_config']['sequence_length'],
                            batch_size=config['offline_config']['batch_size'], config=config[
                                'tcpspec'], shuffle_buffer_size=config['offline_config']['shuffle_buffer_size'],
                            should_shuffle=config['offline_config']['should_shuffle'],
                            interleave_type=config['offline_config']['interleave_type'],
                            block_length_=config['offline_config']['block_length'])
    dataset = dataset.map(lambda sample: sample.data)

    from sage_rl.offline.crr2 import RCRRLearner
    from acme.utils import counting
    counter = counting.Counter(prefix="LR")
    learner = RCRRLearner(
        policy_network=policy_network,
        critic_network=critic_network,
        target_policy_network=target_policy_network,
        target_critic_network=target_critic_network,
        dataset=dataset,
        counter=counter,
        discount=config['offline_config']['discount'],
        policy_lr=config['offline_config']['policy_lr'], critic_lr=config['offline_config']['critic_lr'],
        policy_improvement_modes=config['offline_config']['policy_improvement_modes'],
        logger=logger,
        checkpoint=config['use_learner_ckpt'], save_every_n_hours=config['save_every_n_hours'],
        target_update_period=config['offline_config']['target_update_period'], add_uid=config['offline_config']['add_uid'],
        max_grad_norm=config['offline_config']['max_grad_norm'],)

    with open(os.path.join(learner._checkpointer._checkpoint_dir, "config-rl.yaml"), "w") as fs:
        yaml.dump(config, fs)

    for _ in range(config['offline_config']['num_training_step']):
        learner.step()
        # log loss to csv:
        logger._to._to._to[0]._file.flush()


if __name__ == "__main__":

    run_crr()
