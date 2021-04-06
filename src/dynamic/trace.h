#ifndef TRACE_H
#define TRACE_H

#include <sys/types.h>
#include <sys/stat.h>

#include "mpi.h"
#include "typedef.h"

#define MPI_FUNCTIONS 127 

void TRACE_INIT(int, int);
void TRACE_START(int);
void TRACE_STOP(int);
void TRACE_FLUSH();
void TRACE_FINISH();
void TRACE_P2P(int source, int dest, int tag, int commid, int size);
void TRACE_WAIT(_uint64 id);
void TRACE_WAITALL(_uint32 count, _uint64* buf);
void TRACE_COMM(int commid, int count, int*buf);
void TRACE_COLL(int sendbytes, int recvbytes, int commid, int root);

extern int mpi_pattern(const int pattern,const int id);
extern int mpi_pattern_exit(const int pattern,const int id);

extern int proc_id;
extern _uint64 trace_id;

typedef struct{
	_uint16  mpi_id;	/*MPI event ID*/
	_uint16  comm_id;	/*Communicator ID*/
	_int16   src;
	_int16   tag;
	_uint16  dest;          /*The destination of msg.*/
	_uint32  length;	/*Message size in Byte*/
	double   entry_time;	/*Time in us;*/
	double   exit_time;
	_uint64  id;		/*Unique ID for MPI routine*/
}p2p_msg;

typedef struct{
	_uint16  mpi_id;	/*MPI event ID*/	
	_uint16	 comm_id;
	_uint16	 root_id;	/*The ID for root process*/
	_uint32	 send_bytes;
	_uint32	 recv_bytes;
	double   entry_time;	/*Time in us;*/
	double   exit_time;
	_uint64  id;		/*Unique ID for each MPI routine*/
}coll_msg;

typedef struct{
	_uint16  mpi_id;
	_uint16  comm_id;	/*The ID for MPI_COMM_WORLD*/
	_uint16  proc_size;
	double   exit_time;	/*Time in us;*/
}trace_info;

typedef struct{
	_uint16  mpi_id;
	_uint64  p2p_id;
	_uint64	 id;
	double   entry_time;	/*Time in us;*/
	double   exit_time;
}wait_eve;

typedef struct{
	_uint16 mpi_id;
	_uint32	count;
	_uint64 id;
	double entry_time;
	double exit_time;
}waitall_eve;

typedef struct{
	_uint16 mpi_id;
	_uint16 comm_id;
	_uint32 count;
	_uint64 id;
}comm_eve;

#endif /*TRACE_H*/

