set term postscript color eps enhanced 22
set output "queues-moleson.eps"

set size 0.95,0.6

X=0.1
W=0.26
M=0.025

load "styles.inc"

set tmargin 11.8
set bmargin 2.5

set multiplot layout 1,3

unset key

set grid ytics

set xtics ("" 1, "" 2, "" 4, 8, "" 16, 32, "" 48, 64, 96, 128) nomirror out offset -0.25,0.5 font "Helvetica Condensed"
set label at screen 0.5,0.03 center "Number of threads" font "Helvetica Condensed"
#set label at screen 0.5,0.565 center "Queues with 10^{7} pairs of enqueues/dequeues"


# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 2.5,0 "% throughput (/HP)"
set ytics 40 offset 0.5,0                                 
set yrange [0:200]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "Michael-Scott"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/moleson/q-ll.txt' using 1:(100*$2/$2)  with linespoints notitle ls 7 lw 3 dt (1,1), \
    '../data/moleson/q-ll.txt' using 1:(100*$3/$2)  with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/moleson/q-ll.txt' using 1:(100*$4/$2)  with linespoints notitle ls 2 lw 3 dt 1, \
	'../data/moleson/q-ll.txt' using 1:(100*$6/$2)  with linespoints notitle ls 8 lw 3 dt 1

unset ylabel
set ytics format ""

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W


#set style textbox opaque noborder fillcolor rgb "white"
#set label at first 1,2.5 front boxed left offset -0.5,0 "2.5" font "Helvetica Condensed"
unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "LCRQ"

plot \
    '../data/moleson/q-ll.txt' using 1:(100*$7/$7)  with linespoints notitle ls 7 lw 3 dt (1,1), \
    '../data/moleson/q-ll.txt' using 1:(100*$8/$7)  with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/moleson/q-ll.txt' using 1:(100*$9/$7)  with linespoints notitle ls 2 lw 3 dt 1, \
	'../data/moleson/q-ll.txt' using 1:(100*$11/$7) with linespoints notitle ls 8 lw 3 dt 1

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset label
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "TurnQueue"

plot \
    '../data/moleson/q-ll.txt' using 1:(100*$12/$12)  with linespoints notitle ls 7 lw 3 dt (1,1), \
    '../data/moleson/q-ll.txt' using 1:(100*$13/$12)  with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/moleson/q-ll.txt' using 1:(100*$14/$12)  with linespoints notitle ls 2 lw 3 dt 1, \
	'../data/moleson/q-ll.txt' using 1:(100*$16/$12)  with linespoints notitle ls 8 lw 3 dt 1



# Second row

unset tics
unset border
unset xlabel
unset ylabel
unset label

set key at screen 0.93,0.11 font ",18" samplen 2.0 bottom
plot [][0:1] \
    2 with linespoints title 'hp'        ls 7 lw 4 dt (1,1), \
    2 with linespoints title 'ptb'       ls 1 lw 4 dt (1,1), \
    2 with linespoints title 'ptp'       ls 2 lw 4 dt 1, \
    2 with linespoints title 'orc'       ls 8 lw 4 dt 1
    
