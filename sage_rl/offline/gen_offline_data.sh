if [ $# -ne 2 ]
then

    echo "usage: ./$0 [src: dir including <files_tmp_*> folders] [dst]"
    exit
fi


source ~/venvpy36/bin/activate

src="$1"
dst="$2"
pids=""
for i in `ls $src | grep files_tmp_`
do
    python gen_offline_data.py --in_data_dir=$src/$i/ --dataset_name=$dst &
    #sleep 1
    pids="$pids $!"
done
for pid in $pids
do
    wait $pid
done

#
#for i in `ls $src | grep files_tmp_`;
#do
#    echo "rm -rv $src/$i &"
#done

deactivate
