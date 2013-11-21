
set title 'Sheriff Runtime Overhead'
# can also pass "fontsize 12" to fig terminal
set terminal fig color depth 98

set xlabel 'Benchmark' 0,4
set ylabel 'Normalized Execution Time' 1.5,0
set xtics rotate by -45 ("histogram" 0.84375, "kmeans" 2.00625, "linear_regression" 3.16875, "matrix_multiply" 4.33125, "pca" 5.49375, "reverse_index" 6.65625, "string_match" 7.81875, "word_count" 8.98125, "blackscholes" 10.14375, "canneal" 11.30625, "dedup" 12.46875, "ferret" 13.63125, "fluidanimate" 14.79375, "freqmine" 15.95625, "streamcluster" 17.11875, "swaptions" 18.28125, "hmean" 19.44375)
set format y "  %g"

set boxwidth 0.375
set xrange [0:20.29]
set yrange[0:2]
set grid noxtics ytics

set label "3.26" at 14.2,2.1 right font "Times,10"
set size 1.2,1
set label "BARGRAPH_TEMP_" at 1,1
set label "BARGRAPH_TEMP_pthreads" at 1,1
set label "BARGRAPH_TEMP_Sheriff" at 2,1

plot  '-' notitle with boxes lt 3, '-' notitle with boxes lt 4
0.65625, 1.0
1.81875, 1.0
2.98125, 1
4.14375, 1
5.30625, 1
6.46875, 1
7.63125, 1
8.79375, 1
9.95625, 1
11.11875, 1
12.28125, 1
13.44375, 1
14.60625, 1
15.76875, 1
16.93125, 1
18.09375, 1
19.25625, 1
e
1.03125, 0.77
2.19375, 1.05
3.35625, 0.12
4.51875, 1.01
5.68125, 1.03
6.84375, 1.55
8.00625, 1.0
9.16875, 1.02
10.33125, 1
11.49375, 1.07
12.65625, 1.27
13.81875, 3.26
14.98125, 1.25
16.14375, 1.01
17.30625, 1.10
18.46875, 0.96
19.63125, 0.729244076413584
e
