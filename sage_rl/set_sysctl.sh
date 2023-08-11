# allow testing with buffers up to 128MB
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.wmem_max=134217728
# increase Linux autotuning TCP buffer limit to 64MB
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 67108864"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 67108864"
# increase the length of the processor input queue
sudo sysctl -w  net.core.netdev_max_backlog=250000

sudo sysctl -w -q net.ipv4.tcp_low_latency=1
sudo sysctl -w -q net.ipv4.tcp_autocorking=0
sudo sysctl -w -q net.ipv4.tcp_no_metrics_save=1
sudo sysctl -w -q net.ipv4.ip_forward=1


# What happaned?! mahimahi couldn't make enough interfaces ==> some servers couldn't be initiated!
# Solution: increase max of inotify
sudo sysctl -w -q fs.inotify.max_user_watches=524288
sudo sysctl -w -q fs.inotify.max_user_instances=524288

#Remove Previous Shared Memories:
user=`whoami`
a=`ipcs -m | grep "root\|$user" | awk '{if($4==666) print $2}'`
for i in $a; do echo $i; sudo ipcrm shm $i; done
echo "Shared Memories are cleared"
