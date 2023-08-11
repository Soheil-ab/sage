
#from utils import CSVLogger


import argparse

from tqdm import tqdm
from ruamel.yaml import YAML


import matplotlib.pyplot as plt

import tensorflow as tf
import pandas as pd

from os import listdir

import numpy as np
import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'


RUNMODE = 0  #  0:eval


os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
tf.config.set_visible_devices([], 'GPU')


physical_devices = tf.config.list_physical_devices('GPU')
try:
    tf.config.experimental.set_memory_growth(physical_devices[0], True)
except:
    # Invalid device or cannot modify virtual devices once initialized.
    pass
yaml = YAML()


if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    parser.add_argument('--mem_r', type=int, default=123456)
    parser.add_argument('--mem_w', type=int, default=12345)
    parser.add_argument('--base_path', type=str, required=False, default='')
    parser.add_argument('--eval', action='store_true', default=False,
                        help='(Deprecated 9/23)default is  %(default)s')

    parser.add_argument('--mode', type=int, required=True, default=0)
    parser.add_argument('--flows', type=int, required=True, default=0)
    parser.add_argument('--bw', type=int, required=True, default=0)
    parser.add_argument('--id', type=int, required=True, default=0)
    args = parser.parse_args()

    if args.mode == 0:

        from tcpactor_evalcrr import run_evalcrr as run_eval
        print("[TCPACTOR.PY][EVAL] Actor %s is starting ..." % (args.id))
        run_eval(args)
