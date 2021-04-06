#!/bin/bash
run:
	mpicxx -g -O0 main.c -o main.org
	mpirun -np 2 ./main.org

clean:
	rm -f *.s *.o *.bc main.org main.cypress out.txt mpi_trace* intra_compressed_trace*
