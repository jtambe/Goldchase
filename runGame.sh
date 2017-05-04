#! /bin/bash

rm -f /dev/shm/sem.semSHM
rm -f /dev/shm/mapSHM

make

./test_prg


