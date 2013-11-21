
set title 'Sheriff Runtime Overhead'
# can also pass "fontsize 12" to fig terminal
set terminal fig color depth 98

set xlabel 'Benchmark' 0,4
set ylabel 'Normalized Execution Time' 1.5,0
set xtics rotate by -45 ("histogram" 0.84375, "kmeans" 2.00625, "blackscholes" 3.16875, "canneal" 4.33125)
set format y "  %g"

set boxwidth 0.375
set xrange [0:5.18]
set yrange[0:2]
set grid noxtics ytics

set label "3.26" at 14.2,2.1 right font "Times,10"
set label "BARGRAPH_TEMP_" at 1,1
set label "BARGRAPH_TEMP_pthreads" at 1,1
set label "BARGRAPH_TEMP_Sheriff" at 2,1

plot  '-' notitle with boxes lt 3, '-' notitle with boxes lt 4
0.65625, 1.0
1.81875, 1.0
2.98125, 1
4.14375, 1
e
1.03125, 0.77
2.19375, 1.05
3.35625, 1
4.51875, 1.07
e
