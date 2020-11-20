set term postscript color eps enhanced 22
set output "linkedlists-castor.eps"

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

set xtics ("" 1, "" 2, "" 4, 8, 16, 32, "" 40, 64, 80) nomirror out offset -0.25,0.5 font "Helvetica Condensed"
set label at screen 0.5,0.03 center "Number of threads" font "Helvetica Condensed"
#set label at screen 0.5,0.565 center "Michael-Harris linked list set with 10^{3} keys"


# First row

set lmargin at screen X
set rmargin at screen X+W

set ylabel offset 2.5,0 "Operations ({/Symbol \264}10^6/s)"
set ytics 0.5 offset 0,0                                 
set yrange [0:1.8]

set label at graph 0.5,1.075 center font "Helvetica-bold,18" "  i=50% r=50% l=0%"

set key at graph 0.99,0.99 samplen 1.5

plot \
    '../data/castor/set-ll-1k-mh-hp.txt'  using 1:($2/1e6)  with linespoints notitle ls 7 lw 3 dt (1,1), \
    '../data/castor/set-ll-1k-mh-ptb.txt' using 1:($2/1e6)  with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/castor/set-ll-1k-mh-ptp.txt' using 1:($2/1e6)  with linespoints notitle ls 2 lw 3 dt 1, \
	'../data/castor/set-ll-1k-mh-orc.txt' using 1:($2/1e6)  with linespoints notitle ls 8 lw 3 dt 1
#    '../data/castor/set-ll-1k-mh-ttp.txt' using 1:($2/1e6)  with linespoints notitle ls 3 lw 3 dt 1, \

unset ylabel
set ytics format ""
set yrange [0:2.7]

set lmargin at screen X+(W+M)
set rmargin at screen X+(W+M)+W


unset label
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,2.7 front boxed left offset -0.5,0 "2.7"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "  i=5% r=5% l=90%"

plot \
    '../data/castor/set-ll-1k-mh-hp.txt'  using 1:($3/1e6) with linespoints notitle ls 7 lw 3 dt (1,1), \
    '../data/castor/set-ll-1k-mh-ptb.txt' using 1:($3/1e6) with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/castor/set-ll-1k-mh-ptp.txt' using 1:($3/1e6) with linespoints notitle ls 2 lw 3 dt 1, \
 	'../data/castor/set-ll-1k-mh-orc.txt' using 1:($3/1e6) with linespoints notitle ls 8 lw 3 dt 1
#   '../data/castor/set-ll-1k-mh-ttp.txt' using 1:($3/1e6) with linespoints notitle ls 3 lw 3 dt 1, \

set lmargin at screen X+2*(W+M)
set rmargin at screen X+2*(W+M)+W

unset ylabel
set ytics format ""
set yrange [0:3.0]

unset label
set style textbox opaque noborder fillcolor rgb "white"
set label at first 1,3.0 front boxed left offset -0.5,0 "3"
set label at graph 0.5,1.075 center font "Helvetica-bold,18" "  i=0% r=0% l=100%"

plot \
    '../data/castor/set-ll-1k-mh-hp.txt'  using 1:($4/1e6)  with linespoints notitle ls 7 lw 3 dt (1,1), \
    '../data/castor/set-ll-1k-mh-ptb.txt' using 1:($4/1e6)  with linespoints notitle ls 1 lw 3 dt (1,1), \
    '../data/castor/set-ll-1k-mh-ptp.txt' using 1:($4/1e6)  with linespoints notitle ls 2 lw 3 dt 1, \
	'../data/castor/set-ll-1k-mh-orc.txt' using 1:($4/1e6)  with linespoints notitle ls 8 lw 3 dt 1
#    '../data/castor/set-ll-1k-mh-ttp.txt' using 1:($4/1e6)  with linespoints notitle ls 3 lw 3 dt 1, \


unset tics
unset border
unset xlabel
unset ylabel
unset label

set key at screen 0.34,0.12 font ",18" samplen 2.0 bottom 
plot [][0:1] \
    2 with linespoints title 'hp'        ls 7 lw 4 dt (1,1), \
    2 with linespoints title 'ptb'       ls 1 lw 4 dt (1,1), \
    2 with linespoints title 'ptp'       ls 2 lw 4 dt 1, \
    2 with linespoints title 'orc'     ls 8 lw 4 dt 1
#    2 with linespoints title 'TTP'       ls 3 lw 4 dt 1, \
    
