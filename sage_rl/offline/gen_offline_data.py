from absl import flags

from threading import Thread

import multiprocessing
import reverb
from acme.adders import reverb as adders

from typing import Dict, Set


import numpy as np
import os
import pandas as pd
import tensorflow as tf

from os import listdir
import argparse
import re
import tree
from ruamel.yaml import YAML
import csv
import sys
yaml = YAML()


FLAGS = flags.FLAGS


def process_files(in_data_dir, out_dataset_name, config, files_dict, result_dir, thread_id):

    obs_cols = config['tcpspec']['obs_cols']
    reward_cols = config['tcpspec']['reward_cols']
    # Original raw data's last two columns must be [reward, cwndrate (prev_action)]
    prev_action_cols = [reward_cols[0]+1]
    action_cols = [reward_cols[0]+2]
    discount_cols = [reward_cols[0]+3]

    # load txt and generate preprocessed txt
    for i, train_filename in enumerate(files_dict):

        try:
            examples = pd.read_csv(
                files_dict[train_filename], header=None, sep='\s+')
        except:
            print(f"problem with {train_filename}:{i}", train_filename, i)
            continue

        examples = examples.rename(
            columns={0: 'timestamp', reward_cols[0]: 'reward'})

        LOG2 = False
        if config['tcpspec']['action_version'] == 9:
            LOG2 = True

        if LOG2:
            examples[prev_action_cols] = np.round(
                np.log2(examples[prev_action_cols]), 3)
        # Action column:
        # we process the action here by shifting the original action {t} to {t-1}, and drop the last one
        examples['action'] = examples[prev_action_cols].shift(
            periods=-1, fill_value=0.0)
        examples = examples[:-1]

        DEBUG = False
        if DEBUG:
            examples['discount'] = float(i)  # 1.0
        else:
            examples['discount'] = 1.0

        # sanity check index is correct
        assert [examples.columns.get_loc(
            "action")] == action_cols, "something is wrong"
        assert [examples.columns.get_loc(
            "discount")] == discount_cols, "something is wrong"

        examples = examples[[*obs_cols, "reward", "action", "discount"]]
        # check null
        if examples.isnull().values.any():
            print(f"{train_filename}:{i} has null value, skip",
                  train_filename, i)

            continue
        out_filename = os.path.join(result_dir, train_filename)
        examples.to_csv(out_filename, header=True, index=False)

    print("Done with ", in_data_dir, " part ", thread_id)


def make_offline_csv(
        in_data_dir: str,
        out_dataset_name: str = 'simplev101', configfile=None):

    # convert data in <in_data_dir> to new format and generate data at <out_dataset_name>

    num_cores = multiprocessing.cpu_count()

    if configfile:
        with open(configfile, 'r') as fs:
            config = yaml.load(fs)
    else:
        with open("configs/config-rl.yaml", 'r') as fs:
            config = yaml.load(fs)

    target_dirs = os.path.join(in_data_dir, "")
    files_dict = {}
    threads_files = {}
    for i in range(num_cores):
        threads_files[i] = {}
    file_index = 0
    for (dirpath, dirnames, filenames) in os.walk(target_dirs):
        for file in filenames:
            if file.endswith(".txt"):
                k = file
                v = os.path.join(dirpath, file)
                threads_files[file_index % num_cores].update({k: v})
                file_index = file_index + 1

    if "/" in out_dataset_name:
        result_dir = out_dataset_name
    else:
        result_dir = os.path.join("offline-datasets", out_dataset_name)

    os.makedirs(result_dir, exist_ok=True)

    threads = []
    valid_threads = num_cores
    if file_index < num_cores:
        valid_threads = file_index

    for i in range(valid_threads):
        try:
            print("Processing ", in_data_dir, "    ...")
            tmp = Thread(target=process_files, args=(in_data_dir,
                                                     out_dataset_name, config, threads_files[i], result_dir, i,))
            threads.append(tmp)
            tmp.start()
        except:
            print("Thread ", i, " failed!")
    for thread in threads:
        thread.join()


def build_dataset(
        dataset_path_or_name: str = None,
        seq_len: int = 2,
        batch_size: int = 3,
        config=None,
        shuffle_buffer_size: int = 1000000,
        block_length_: int = 1,
        should_shuffle: bool = True,
        interleave_type: str = "none",):

    # make a new dict
    if os.path.isdir(dataset_path_or_name):
        target_dataset_dir = dataset_path_or_name
    else:
        target_dataset_dir = os.path.join(
            os.getcwd(), 'offline-datasets', dataset_path_or_name)

    if os.path.isdir(target_dataset_dir) != True:
        raise ValueError(
            f"Target dataset path not exists: {target_dataset_dir}")

    files_dict = {}
    for (dirpath, dirnames, filenames) in os.walk(target_dataset_dir):
        for file in filenames:
            if file.endswith(".txt"):
                k = file
                v = os.path.join(dirpath, file)
                files_dict.update({k: v})

    filenames = [*files_dict]
    filenames = [os.path.join(target_dataset_dir, filename)
                 for filename in [*files_dict]]
    filenames = filenames

    obs_dim = config['obs_dim']
    total_cols_num = obs_dim + 1 + 1 + 1
    shapes = [tf.float32]*(total_cols_num)

    extra_spec = {
        'core_state': tf.zeros([1], dtype=tf.float32),
    }

    def _sequence_zeros(spec):
        return tf.zeros([seq_len] + spec.shape, spec.dtype)
    e_t = tree.map_structure(_sequence_zeros, extra_spec)

    def parse_example(*timestep_data):
        ot = timestep_data[0:obs_dim]
        ot = tf.stack(ot, axis=0)
        rt = timestep_data[obs_dim]
        at = [timestep_data[obs_dim+1]]
        dt = timestep_data[obs_dim+2]

        timestep_restructured = {'observation': ot,
                                 'action': at, 'reward': rt, 'discount': dt}
        return timestep_restructured

    def build_seq(sequences):
        data = adders.Step(
            observation=sequences['observation'],
            action=sequences['action'],
            reward=sequences['reward'],
            discount=sequences['discount'],
            start_of_episode=(),
            # extras=e_t)
            extras=())

        key = tf.zeros([seq_len], tf.uint64)
        probability = tf.ones([seq_len], tf.float64)
        table_size = tf.ones([seq_len], tf.int64)
        priority = tf.ones([seq_len], tf.float64)
        info = reverb.SampleInfo(
            key=key,
            probability=probability,
            table_size=table_size,
            priority=priority)

        return reverb.ReplaySample(info=info, data=data)

    file_ds = tf.data.Dataset.from_tensor_slices(filenames)
    if should_shuffle == True:

        file_ds = file_ds.repeat().shuffle(len(filenames))

        if interleave_type == "full":
            example_ds = file_ds.interleave(lambda x: tf.data.experimental.CsvDataset(x, record_defaults=shapes, field_delim=',', header=True).map(
                parse_example).batch(seq_len, drop_remainder=True), cycle_length=tf.data.AUTOTUNE, block_length=block_length_, num_parallel_calls=tf.data.AUTOTUNE)
        elif interleave_type == "none":
            example_ds = file_ds.interleave(lambda x: tf.data.experimental.CsvDataset(x, record_defaults=shapes, field_delim=',', header=True).map(
                parse_example).batch(seq_len, drop_remainder=True), cycle_length=len(filenames), block_length=len(filenames))
        else:
            print("Invalid interleave_type:", interleave_type)
            return

        example_ds = example_ds.map(
            build_seq, num_parallel_calls=tf.data.experimental.AUTOTUNE)
        example_ds = example_ds.shuffle(
            shuffle_buffer_size, reshuffle_each_iteration=True)
    else:
        file_ds = file_ds.repeat()
        if interleave_type == "full":
            example_ds = file_ds.interleave(lambda x: tf.data.experimental.CsvDataset(x, record_defaults=shapes, field_delim=',', header=True).map(
                parse_example).batch(seq_len, drop_remainder=True), cycle_length=tf.data.AUTOTUNE, block_length=block_length_, num_parallel_calls=tf.data.AUTOTUNE)
        elif interleave_type == "none":
            example_ds = file_ds.interleave(lambda x: tf.data.experimental.CsvDataset(x, record_defaults=shapes, field_delim=',', header=True).map(
                parse_example).batch(seq_len, drop_remainder=True), cycle_length=len(filenames), block_length=len(filenames))
        else:
            print("Invalid interleave_type:", interleave_type)
            return

        example_ds = example_ds.map(
            build_seq, num_parallel_calls=tf.data.experimental.AUTOTUNE)

    example_ds = example_ds.batch(batch_size)
    example_ds = example_ds.prefetch(10)
    return example_ds


def main():

    parser = argparse.ArgumentParser()
    parser.add_argument('--in_data_dir', type=str, required=True)
    parser.add_argument('--dataset_name', type=str, required=True)
    parser.add_argument('--config', type=str, default=None,
                        help='None: use the default config-rl.yaml (in this folder!); Otherwise, use the specificed one')
    # Let argparse parse known flags from sys.argv.
    args, unknown_flags = parser.parse_known_args()
    flags.FLAGS(sys.argv[:1] + unknown_flags)  # Let absl.flags parse the rest.

    examples = make_offline_csv(
        args.in_data_dir, args.dataset_name, args.config)


if __name__ == "__main__":

    main()
