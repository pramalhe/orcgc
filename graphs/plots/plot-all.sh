#!/bin/sh

for i in \
linkedlists-castor.gp \
linkedlists-moleson.gp \
linkedlists-orc-castor.gp \
linkedlists-orc-moleson.gp \
queues-castor.gp \
queues-moleson.gp \
trees-castor.gp \
trees-moleson.gp \
;
do
  echo "Processing:" $i
  gnuplot $i
  epstopdf `basename $i .gp`.eps
  rm `basename $i .gp`.eps
done
