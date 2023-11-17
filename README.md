# Sage
### or How Computers Can Learn from Existing Schemes and Master Internet CC!
This repository offers the Sage framework, initially introduced in our [SIGCOMM'23 paper](https://dl.acm.org/doi/10.1145/3603269.3604838). You can check out a 10-min recorded presentation of Sage [here](https://www.youtube.com/watch?app=desktop&v=_3-n8_1JwnU&ab_channel=ACMSIGCOM). 

# Outline
- [What Is Included](#what-is-included)
- [Prerequisites](#prerequisites)
- [Installing Kernel Patches](#installing-kernel-patches)
- [Installing Mahimahi Patches](#installing-mahimahis-patches)
- [Installing Sage](#installing-sage)
- [Running Sage](#running-sage)
- [Creating Dataset](#creating-dataset)
- [Training Sage](#training-sage)
- [Citation Guidelines](#citation-guidelines)

# What Is Included
This repository includes various elements: 
1) Linux Kernel Debian packages implementing Sage (TCP Pure), along with [Orca](https://github.com/Soheil-ab/Orca), [DeepCC](https://github.com/Soheil-ab/DeepCC.v1.0), [C2TCP](https://github.com/Soheil-ab/c2tcp), & [NATCP](https://www.usenix.org/conference/hotedge19/presentation/abbasloo).
2) Sage's source code including the Data-Driven RL module, general representation block, and policy enforcer.
3) A pre-trained Sage model which is the outcome of a 7-day training session.
4) Tools for dataset generation (compatible with Sage training).
5) Necessary codes to generate Sage-governed flows, collect results, and create figures.
6) A standalone version of Sage's server/client application.

# Prerequisites
Initial setup involves the following steps:

1) Installation of a patched Linux kernel to enable TCP PURE.
2) Installation of a patched version of the emulator Mahimahi.
3) Installation of required packages for Sage.

```bash
cd ~ && git clone https://github.com/soheil-ab/sage.git
```
## Installing Kernel Patches
The provided patch has been tested on __Ubuntu 16.04__. Thus, it's recommended to install Ubuntu 16.04 as the base and then apply the Sage Kernel patch.

To install Sage's Kernel, simply follow these steps:
```bash
cd ~/sage/linux-patch/
sudo dpkg -i linux-image-4.19.112-0062_4.19.112-0062-10.00.Custom_amd64.deb
sudo dpkg -i linux-headers-4.19.112-0062_4.19.112-0062-10.00.Custom_amd64.deb
sudo reboot 
```

### Verifying the new kernel
After installing the Sage's kernel and restarting your system, check if the system is using the new kernel:

```bash
uname -r
```

If the output isn't 4.19.112-0062, you should prioritize the installed Kernel in the grub list. For instance, you can use the grub-customizer application. Install the grub-customizer using the following. Then, move the 4.19.112-0062 kernel to the top of the list (first row).

```bash
sudo add-apt-repository ppa:danielrichter2007/grub-customizer
sudo apt-get update
sudo apt-get install grub-customizer
sudo grub-customizer
```

## Installing Mahimahi's Patches
Install the required Mahimahi patches as per the instructions provided in [ccBench](https://github.com/Soheil-ab/ccBench). Make sure to update Mahimahi (with the provided patches) before running Sage.

## Installing Sage
Sage is developed and tested on Python 3.6, and built on top of TensorFlow 2.

Begin by installing necessary system tools:
```bash
bash install_packages.sh
```

To install the required Python packages and manage the dependencies, set up a Python environment using a virtual environment. 

```bash
virtualenv --python=python3.6 ~/venvpy36
```

Now, activate the environment:
```bash
source ~/venvpy36/bin/activate
```

Then, install required packages:
```bash
cd ~/sage
pip install --upgrade pip
pip install --no-deps -r requirements.txt
pip install -r requirements.txt
```

Now, install sage module:
```bash
cd ~/sage
pip install -e .
```

Compile/Build Sage and set the system parameters:
```bash
cd ~/sage/sage_rl
bash build.sh
bash set_sysctl.sh
```


# Running Sage
Here, a standalone Sage implementation is provided as a server and client application and the ```models``` directory contains a pre-trained model from a 7-day training session.

To use the provided model for a standalone evaluation, first copy the model to the desired path. 
```bash
cd ~/sage/sage_rl
bash cp_models.sh
```

Do a sanity check to make sure that the model can be loaded correctly:
```bash
python rl_module/test_loading_sage_model.py
```


To run an experiment with Sage, for example in a 24Mbps link with 20ms min RTT, run the following:
```bash
cd ~/sage/sage_rl
bash run_sample.sh
```

# Creating the Dataset
Dataset creating involves two main steps: 
1) Using the Policy Collector to generate the raw dataset.
2) Converting the raw dataset to the format required by Sage's data-driven training.

## 1. Using the Policy Collector
To start, we need to install the Policy Collector. We have provided a version of Policy Collector as part of our [pantheon-modified repository](https://github.com/Soheil-ab/ccBench). To install it, simply follow the instructions provided [there](https://github.com/Soheil-ab/ccBench). 

Now, let's generate a simple dataset representing TCP Vegas' behavior in a scenario. In particular, we will bring up a network with 48Mbps link capacity, minimum RTT of 20ms, and a fifo bottleneck queue with a size of 1000 packets. Then, we will send one flow for the 30s using TCP Vegas as its CC scheme. 
```bash
cd ~/pantheon-modified/src/experiments/
./cc_dataset_gen_solo.sh vegas single-flow-scenario 1 1 0 10 1000 0 48 30 48 48
```
After running the above, you can inspect the generated raw dataset:
```bash
vi ~/pantheon-modified/third_party/tcpdatagen/dataset/vegas_wired48_10_1000_0_cwnd.txt
``` 
Moreover, you can find the general log of the experiment at ```~/pantheon-modified/src/experiments/data/single-flow-scenario-vegas-wired48-10-1000-0/```

## 2. From Raw to Final Dataset
Assuming the data is located at ```~/raw_dataset/*.txt.```, we need to convert it to the format Sage's data-driven training requires: 

```bash
cd ~sage/sage_rl/offline
python gen_offline_data.py --in_data_dir=~/raw_dataset --dataset_name=~/final_dataset/ --config=configs/config-rl-offline.yaml
```
The final dataset will be saved at ```~/final_dataset/```. You can find the configuration file at ```./configs/config-rl-offline.yaml``` and customize it to match your needs. 

# Training Sage
After creating the final dataset (e.g., ```~/final_dataset/*.txt.```), you can train a Sage model using the generated dataset:

```bash
cd ~sage/sage_rl/offline
python offline_agent.py  --dataset_name=~/final_dataset --config=configs/config-rl-offline.yaml
```

Once again, you can find the configuration file at ```./configs/config-rl-offline.yaml``` and customize it to match your needs. Training logs will be stored in the designated folder.


# Citation Guidelines
 
Please use the following citation format to cite this repository: 
```bib
@inproceedings{sage,
  title={Computers Can Learn from the Heuristic Designs and Master Internet Congestion Control},
  author={Yen, Chen-Yu and Abbasloo, Soheil and Chao, H Jonathan},
  booktitle={ACM SIGCOMM 2023 Conference (ACM SIGCOMM '23)},
  year={2023}
}
```
