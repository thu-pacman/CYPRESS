GXX=g++-4.6
LLVM=${GXX} `llvm-config --cxxflags` -g
DLL=-shared -fPIC
OBJS=cfa.o core.o util.o
#For C analysis, just comment the following line
MACRO=-DLLVM_FORTRAN

#mpi.so is the analysis library implemented as LLVM pass
mpi.so: ${OBJS} 
	${GXX} ${DLL} ${OBJS} -o mpi.so

.cpp.o: 
	${LLVM} -c $< -o $@ ${MACRO}

cfa.o: cfa.cpp cfa.h
core.o: core.cpp core.h cfa.h
util.o: util.cpp core.h

.PHONY: clean
clean:
	rm *.o *.so

