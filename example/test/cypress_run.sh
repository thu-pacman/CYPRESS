#!/bin/bash

CYPRESS_DIR=/mnt/home/jinyuyang/MY_PROJECT/CYPRESS
MPI_DIR=/mnt/home/jinyuyang/build/intel/impi/4.1.0.027
GCC=gcc

clang -c -emit-llvm -I${MPI_DIR}/include main.c -o main.bc
#llvm-as main.s -o main.bc
cp ${CYPRESS_DIR}/src/static/in.txt ./
opt -load ${CYPRESS_DIR}/src/static/mpi.so -mpi < main.bc > main.o                                                                              
llc main.o -o main.o.s                                                                                                    
${GCC} main.o.s -c -o main.o.o                                                                                                     
${GCC} main.o.o -o main.cypress -L${CYPRESS_DIR}/src/dynamic -lmpit $(mpicc -show|cut -c5-) -lz
mpirun -np 2 ./main.cypress
