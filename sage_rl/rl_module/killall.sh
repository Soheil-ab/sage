sudo kill -9 $(ps aux | grep tcpactor.py | awk '{ print $2 }')
sudo kill -9 $(ps aux | grep actor.py | awk '{ print $2 }')

sudo kill -9 $(ps aux | grep learner.py | awk '{ print $2 }')

sudo kill -9 $(ps aux | grep varserver.py | awk '{ print $2 }')
sudo kill -9 $(ps aux | grep replay.py | awk '{ print $2 }')
a="`ps aux | grep sage_mlt.sh | awk '{ print $2 }'`"
for i in $a
do
    sudo kill -s9 $i
done
a="`ps aux | grep sage.sh | awk '{ print $2 }'`"
for i in $a
do
    sudo kill -s9 $i
done
a="`ps aux | grep iperf | awk '{ print $2 }'`"
for i in $a
do
    sudo kill -s9 $i
done

sudo killall -9 ray::RemoteVariableClient
sudo killall -9 ray::IDLE

sleep 1


