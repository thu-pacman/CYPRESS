# CYPRESS
CYPRESS: Combining Static and Dynamic Analysis for Top-Down Communication Trace Compression

## Dependencies
- GCC 4.5 or 4.6
- LLVM 3.1 and dragenegg 3.1

   **If you are not using Intel MPI, then there will be some errors about type declaration**

## Build

### Static phase:
```
cd ./src/static
make
```
`mpi.so` is built.
### Dynamic phase:
```
cd ./src/dynamic
make
```
`libmpit.a` is built.

## Usage

For C programs:
```
clang -c -emit-llvm -I${MPI_DIR}/include main.c -o main.bc
opt -load ${CYPRESS_DIR}/src/static/mpi.so -mpi < main.bc > main.o                                                                              
llc main.o -o main.o.s                                                                                                    
${GCC} main.o.s -c -o main.o.o                                                                                                     
${GCC} main.o.o -o main -L${CYPRESS_DIR}/src/dynamic -lmpit $(mpicc -show|cut -c5-) -lz
mpirun -np ${nprocs} ./main
```
For Fortran programs:
```
${GCC} -fplugin=/path/to/dragonegg.so -S -fplugin-arg-dragonegg-emit-ir -I${MPI_DIR}/include main.f -o main.bc
opt -load ${CYPRESS_DIR}/src/static/mpi.so -mpi < main.bc > main.o
llc main.o -o main.o.s
${GFORTRAN} main.o.s -c -o main.o.o
${GFORTRAN} main.o.o -o main -L${CYPRESS_DIR}/src/dynamic -lmpit $(mpicc -show|cut -c5-) -lz
mpirun -np ${nprocs} ./main 
```
Please see `./example/test`, we take an example here.

## Output
An example of "intra_compressed_trace_text_000*":
```
|-branch if id=2, else id=4, total times=1                                                                                        
| 1*T;                                                                                                                            
| |-p2p id=15, total times=1, sum_time=6, sum_sqr_time=36                                                                         
| | comm=64,src=0,tag=0,dest=1,times=1,bytes=(15);                                                                                
|-else id=4                                                                                                                       
| |-branch if id=5, else id=0, total times=0                                                                                      
| |                                                                                                                               
| | |-p2p id=12, total times=0, sum_time=0, sum_sqr_time=0                                                                        
| | |                                                                                                                             
| |-else id=0                                                                                                                     
|-loop id=7, total times=5                                                                                                        
| 1*5;                                                                                                                            
| |-branch if id=8, else id=0, total times=5                                                                                      
| | 5*T;                                                                                                                          
| | |-coll id=40, total times=5, sum_time=155, sum_sqr_time=7757                                                                  
| | | 5*[comm=64,root=65535,sendbytes=-1,recvbytes=-1; ];                                                                         
| |-else id=0 
```


## Publication
J. Zhai, J. Hu, X. Tang, X. Ma and W. Chen, "CYPRESS: Combining Static and Dynamic Analysis for Top-Down Communication Trace Compression," SC '14: Proceedings of the International Conference for High Performance Computing, Networking, Storage and Analysis, New Orleans, LA, USA, 2014, pp. 143-153, doi: 10.1109/SC.2014.17.

## Cite
To cite CYPRESS, you can use the following BibTeX entry:
```
@inproceedings{DBLP:conf/sc/ZhaiHTMC14,
  author    = {Jidong Zhai and
               Jianfei Hu and
               Xiongchao Tang and
               Xiaosong Ma and
               Wenguang Chen},
  editor    = {Trish Damkroger and
               Jack J. Dongarra},
  title     = {{CYPRESS:} Combining Static and Dynamic Analysis for Top-Down Communication
               Trace Compression},
  booktitle = {International Conference for High Performance Computing, Networking,
               Storage and Analysis, {SC} 2014, New Orleans, LA, USA, November 16-21,
               2014},
  pages     = {143--153},
  publisher = {{IEEE} Computer Society},
  year      = {2014},
  url       = {https://doi.org/10.1109/SC.2014.17},
  doi       = {10.1109/SC.2014.17},
  timestamp = {Wed, 16 Oct 2019 14:14:57 +0200},
  biburl    = {https://dblp.org/rec/conf/sc/ZhaiHTMC14.bib},
  bibsource = {dblp computer science bibliography, https://dblp.org}
}
```
