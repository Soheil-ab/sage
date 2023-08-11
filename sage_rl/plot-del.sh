if [ $# -ne 1 ]
then
    echo "usage:./$0 [path to log file]"
fi
out=`echo $1 | sed "s/\.dat/-del\.svg/g"`
cp $1 data
#sed -i "1d" data

cat > plot.gpl << END
reset
set ylabel "Queuing Delay (ms)"
set xlabel "Time (s)"
set autoscale xy
#set logscal y 10
#set terminal svg size 450,250 dynamic enhanced fname 'arial,12'
#set key center outside top horizontal
set style fill solid 0.2 noborder
set terminal svg size 1024,512 fixed  font "Arial,12" solid mouse standalone

#set datafile separator ","
set output "$out"
#set format y "%2.4f"
#set format y "%.2t*10^%+03T"
#set yrange [*:10]
#plot "$1" using 1:2 with p ps 0.2 pt 7  notitle
plot "$1" using 1:2 with l notitle
END
gnuplot plot.gpl
rm plot.gpl data

