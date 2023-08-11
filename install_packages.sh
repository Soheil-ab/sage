
## Installation
## Requires Python >=3.6

sudo apt update
sudo apt install python3-pip
sudo -H python3 -m pip install -U virtualenv



##### Iperf3 for multiple flow scenarios
sudo apt remove -y iperf3 libiperf0
sudo apt install -y libsctp1
cd ~
wget https://iperf.fr/download/ubuntu/libiperf0_3.9-1_amd64.deb
wget https://iperf.fr/download/ubuntu/iperf3_3.9-1_amd64.deb
sudo dpkg -i libiperf0_3.9-1_amd64.deb iperf3_3.9-1_amd64.deb
rm libiperf0_3.9-1_amd64.deb iperf3_3.9-1_amd64.deb
cd -

