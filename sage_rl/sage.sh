if [ $# != 17 ]
then
    echo -e "usage:$0 [latency] [port] [trace downlink] [trace uplink] [iteration ID] [qsize] [Link capacity]"
    echo -e "\t \t[bw2: in case of pulse traces, this indicates BW of the 2nd part, otherwise it's equal the <link capacity>]"
    echo -e "\t \t[trace_period: default 7] [training:1 , evaluation:0] [log prefix] [time] [actor id] [duration: steps] [number of flows] [save] [analyze]"
    echo "you gave $# inputs, but we need 17 inputs!"
    echo "your inputs were $@"
    exit
fi
latency=$1
port=$2
dl=$3
up=$4
it=$5
qsize=$6
bw=$7
bw2=$8
trace_period=${9}
first_time=${10}
scheme="pure"
prefix=${11}
t=${12}
loss="0"
actor_id=${13}
num_steps=${14}
num_flows=${15}
#save the log? 0: do not save, else: save
save=${16}
analyze=${17}
period=10

echo "Actor $actor_id: $dl/$up"

#path=`pwd -P | sed "s/\//___/g"`
#path_to_model="${path}___..___models___config-v9.01-sfl-simple-net1-mlp2-5days___ckpt-dir___"
#sed -i "s/load_ckpt_dir:.*/load_ckpt_dir: ${path_to_model}/g;s/___/\//g" rl_module/config.yaml
#sed -i "s/load_policy_net_sl_dir:.*/load_policy_net_sl_dir: ${path_to_model}/g;s/___/\//g" rl_module/config-rl.yaml

path=`pwd -P`
log="sage-${prefix}-${actor_id}-${dl}-${up}-${latency}-${qsize}-${save}-${bw}-${bw2}-${num_flows}-${it}"
basetimestamp_fld="$path/log/down-${log}_init_timestamp"
echo "base timestamp folder: $basetimestamp_fld"
rm $basetimestamp_fld

echo "will be done in $t-second/$num_steps-Step ..."
sudo -u `logname` ./server.sh $port $period $first_time $scheme "$path/rl_module" $actor_id $dl $up $latency $log $t $loss $qsize $num_steps $basetimestamp_fld $bw $bw2 $trace_period $save $num_flows
echo "server.sh is executed ..."

py_pids=`ps aux | grep "id=$actor_id" | awk -v str="--id=$actor_id" '{if($17==str)print $2}'`
echo "Server (ACTOR $actor_id) is done. closing its python scripts: $py_pids "
for pid in $py_pids
do
    sudo kill -s15 $pid
done

if [[ $num_flows > 1 ]]
then
    for i in `seq 1 $((num_flows-1))`
    do
        iperf_pid=`ps aux | grep "iperf3 -s -p $((port+i))" | awk '{print $2}'`
        echo "Server (ACTOR $actor_id) is done. closing its python scripts: $iperf_pid "
        sudo kill -s9 $iperf_pid
    done
fi
sleep 1

if [[ $analyze == 1 ]]
then
    echo "[Actor $actor_id] Doing Analysis ..."
    echo "------------------------------"
    out="${log}.tr"
    mkdir -p log
    echo $log >> log/$out
    ./mm-thr 500 log/down-${log} 1>log/down-$log.svg 2>res_tmp
    cat res_tmp >>log/$out

    ./mm-del-file log/down-${log} 2>res_tmp2
    ./plot-del.sh log/down-$log.dat
    echo "------------------------------" >> log/$out
    cat res_tmp
    echo "------------------------------"
fi
rm res_tmp res_tmp2

echo "Actor($actor_id) is Finished."
