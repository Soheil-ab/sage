#!/bin/bash

if [ $# != 20 ]
then
    echo -e "[port] [Report Period: 10 msec--fixed for now :)] [First Time: 1=yes(learn), 0=no(continue learning), 2=evaluate] [scheme: pure] [path to run.py] [actor id=0, 1, ...] [downlink] [uplink] [one-way delay] [log_file/comment] [duration] [loss rate] [qsize] [num_steps] [basetimestamp_fld] [bw] [bw2] [trace_period] [save] [num_flows]"

    echo "$@"
    echo "$#"
    exit
fi

port=$1
period=$2
first_time=$3
scheme=$4
path=$5
tid=$6
dl=$7
up=${8}
del=${9}
log=${10}
time=${11}
loss=${12}
qs=${13}
time_training_steps=${14}
basetimestamp_fld=${15}
env_bw=${16}
bw2=${17}
trace_period=${18}
save=${19}
num_flows=${20}

#$path/server $port $path $target $initial_alpha ${period} ${first_time} $scheme $tid $dl $up $del $log $time $loss $qs $time_training_steps $basetimestamp_fld $bw2 $trace_period
$path/sage $port $path $save $num_flows $env_bw ${first_time} $scheme $tid $dl $up $del $log $time $loss $qs $time_training_steps $basetimestamp_fld $bw2 $trace_period


