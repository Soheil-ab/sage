from absl.testing import absltest
from sage_rl.offline.offline_agent import make_networks

from envwapper import TCP_dmEnv_Wrapper
from acme.agents.tf import actors

from ruamel.yaml import YAML
from collections import namedtuple, OrderedDict
from acme.tf import utils as tf2_utils

from acme import specs

import tensorflow as tf
import pandas as pd
from os import listdir


import numpy as np
import os

os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'




os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
tf.config.set_visible_devices([], 'GPU')


yaml = YAML()


class ActorTest(absltest.TestCase):

    def test_loading_sage_model(self):

        config_rl_module_path = os.path.dirname(os.path.realpath(__file__))
        with open(os.path.join(config_rl_module_path, "config-rl-eval.yaml"), 'r') as fs:
            config = yaml.load(fs)

        shrmem_r = None
        shrmem_w = None

        env = TCP_dmEnv_Wrapper(config=config['tcpspec'], for_init_only=False, shrmem_r=shrmem_r,
                                shrmem_w=shrmem_w)
        environment_spec = specs.make_environment_spec(env)

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

        signature = tf.Variable(0, dtype=tf.int32)

        load_path = config['offline_config']['load_ckpt_dir']

        load_path = os.path.normpath(
            os.path.join(config_rl_module_path, load_path))

        print(f"{'='*30} Loading Sage Model From Ckpt_dir: {load_path} {'='*30} ")
        assert tf.train.latest_checkpoint(
            load_path) != None, "no checkpoint is loaded, please choose the ckpt directory"

        checkpointer_rl = tf.train.Checkpoint(
            policy=policy_network, signature=signature)

        checkpointer_rl.restore(
            tf.train.latest_checkpoint(load_path))

        assert signature.numpy(
        ) == 20230806, "wrong signature of sage's final model, please check the ckpt directory"


if __name__ == "__main__":

    absltest.main()
