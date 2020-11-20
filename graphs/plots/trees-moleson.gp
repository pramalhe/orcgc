set term postscript color eps enhanced 22
set output "trees-moleson.eps"

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
#set label at screen 0.5,0.565 center "Lock-free trees and skiplists with 10^{6} keys"


# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 1,0 "Operations ({/Symbol \264}10^6/s)"
set ytics 1 offset 0,0                                 
set yrange [0:2.0]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "  i=50% r=50% l=0%"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/moleson/set-tree-1m-nata-hp.txt'            using 1:($2/1e6)  with linespoints notitle ls 7 lw 3 dt (1,1), \
    '../data/moleson/set-tree-1m-nata-ptb.txt'           using 1:($2/1e6)  with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/moleson/set-tree-1m-nata-ptp.txt'           using 1:($2/1e6)  with linespoints notitle ls 2 lw 3 dt 1, \
    '../data/moleson/set-tree-1m-nata-orc.txt'           using 1:($2/1e6)  with linespoints notitle ls 8 lw 3 dt 1, \
    '../data/moleson/set-skiplist-1m-hsskip-orcorig.txt' using 1:($2/1e6)  with linespoints notitle ls 9 lw 3 dt 1, \
	'../data/moleson/set-skiplist-1m-hsskip-orc.txt'     using 1:($2/1e6)  with linespoints notitle ls 10 lw 3 dt 1

unset ylabel
set ytics format ""
set yrange [0:8]

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W


#set style textbox opaque noborder fillcolor rgb "white"
#set label at first 1,2.5 front boxed left offset -0.5,0 "2.5" font "Helvetica Condensed"
unset label
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,7 front boxed left offset -0.5,0 "7"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "  i=5% r=5% l=90%"

plot \
    '../data/moleson/set-tree-1m-nata-hp.txt'            using 1:($3/1e6) with linespoints notitle ls 7 lw 3 dt (1,1), \
    '../data/moleson/set-tree-1m-nata-ptb.txt'           using 1:($3/1e6) with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/moleson/set-tree-1m-nata-ptp.txt'           using 1:($3/1e6) with linespoints notitle ls 2 lw 3 dt 1, \
	'../data/moleson/set-tree-1m-nata-orc.txt'           using 1:($3/1e6) with linespoints notitle ls 8 lw 3 dt 1, \
    '../data/moleson/set-skiplist-1m-hsskip-orcorig.txt' using 1:($3/1e6)  with linespoints notitle ls 9 lw 3 dt 1, \
	'../data/moleson/set-skiplist-1m-hsskip-orc.txt'     using 1:($3/1e6)  with linespoints notitle ls 10 lw 3 dt 1


set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset ylabel
set ytics format ""
set yrange [0:12]

unset label
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,12 front boxed left offset -0.5,0 "12"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "  i=0% r=0% l=100%"

plot \
    '../data/moleson/set-tree-1m-nata-ptp.txt'           using 1:($4/1e6)  with linespoints notitle ls 2 lw 3 dt 1, \
    '../data/moleson/set-tree-1m-nata-ptb.txt'           using 1:($4/1e6)  with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/moleson/set-tree-1m-nata-hp.txt'            using 1:($4/1e6)  with linespoints notitle ls 7 lw 3 dt (1,1), \
 	'../data/moleson/set-tree-1m-nata-orc.txt'           using 1:($4/1e6)  with linespoints notitle ls 8 lw 3 dt 1, \
    '../data/moleson/set-skiplist-1m-hsskip-orcorig.txt' using 1:($4/1e6)  with linespoints notitle ls 9 lw 3 dt 1, \
	'../data/moleson/set-skiplist-1m-hsskip-orc.txt'     using 1:($4/1e6)  with linespoints notitle ls 10 lw 3 dt 1



# Second row

unset tics
unset border
unset xlabel
unset ylabel
unset label

set key at screen 0.34,0.12 font ",18" samplen 2.0 bottom 
plot [][0:1] \
    2 with linespoints title 'NM-tree-hp'        ls 7 lw 4 dt (1,1), \
    2 with linespoints title 'NM-tree-ptb'       ls 1 lw 4 dt (1,1)

set key at screen 0.65,0.12 font ",18" samplen 2.0 bottom 
plot [][0:1] \
    2 with linespoints title 'NM-tree-ptp'       ls 2 lw 4 dt 1, \
    2 with linespoints title 'NM-tree-orc'     ls 8 lw 4 dt 1
	
set key at screen 0.93,0.12 font ",18" samplen 2.0 bottom 
plot [][0:1] \
    2 with linespoints title 'HS-skip-orc' ls 9 lw 4 dt 1, \
    2 with linespoints title 'skip-orc'     ls 10 lw 4 dt 1
    
