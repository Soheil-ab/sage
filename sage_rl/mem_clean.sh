for i in `ipcs -m | grep -v "key\|--\|root\|0x00000000" | awk '{print $1}'`
do
    echo $i
    ipcrm -M $i
done
