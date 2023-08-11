if [ $# -ne 2 ]
then

    echo "usage:./$0 [path to files -> .../winners-dateset/] [output name]"
    exit
fi
path=$1
out=$2
ls $path > dataset_list
dst="${path}/../$out"
rm -f -r $dst
mkdir -p $dst

total=`cat dataset_list | awk 'END{print NR}'`
rm dataset_list-shuffled

for file in `cat dataset_list`
do
    i=`shuf -i 1-$((total*10)) -n 1`
    echo $i-$file >> tmp
    cp $path/$file $dst/$i-$file
done
sort -k1 -h tmp > dataset_list-shuffled
rm tmp
