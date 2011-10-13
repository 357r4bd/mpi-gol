#!/bin/sh

#
# usage:
#   ./animate.sh #dimension #generations #procs
#
# default init file must be named "init.dat"
#

if [ ! -d frames ]; then
  mkdir frames
fi

rm frames/*

mpicc bde_life.c -DDIM=${1} -DGEN=${2} -lm 
mpirun -np ${3} ./a.out
sleep 1
count=0
for frame in `ls frames`; do
  echo frame $count
  cat frames/$count.dat
  sleep 1
  let count=$count+1
done
