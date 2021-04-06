/*********************************************************
* This program is used to parse the mpi tracing file.
*********************************************************/

#include "typedef.h"
#include "trace.h"
#include "mpiname.h"

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <string.h>

#define  MAXBUF	128*1024

static char trace_buf[MAXBUF];
char* buf_ptr = trace_buf;
_uint16* mpi_id;
p2p_msg* p2p_ip;
trace_info*  trace_ip;
wait_eve* wait_ip;
waitall_eve* waitall_ip;
comm_eve* comm_ip;

int main(int argc, char**argv){

	static gzFile fp_trace = Z_NULL;
	int i, count;
	_uint64* _uint64_ptr;
	_uint32* _uint32_ptr;

	if(argc<2){
		printf("./parse trace_name\n");
		exit(1);	
	}

	char * trace_name = argv[1];

	if ( (fp_trace = gzopen(trace_name, "r")) == Z_NULL){
		printf("Error: Could not open MPI  file %s, Abort!\n", trace_name);
		exit(1);
	}
	
	gzread(fp_trace, (char*)trace_buf, MAXBUF);	

	// Trace title information
	trace_ip =  (trace_info*) buf_ptr;
	printf("MPI_ID:%hd COMM_WORLD_ID:%hd Proc_Size:%hd\n", trace_ip->mpi_id, trace_ip->comm_id, trace_ip->proc_size);
	buf_ptr += sizeof(trace_info);

	mpi_id = (_uint16*) buf_ptr;

	for(;;){

	switch ( *mpi_id ){ 

	case 2: //MPI_Finalize
		buf_ptr += sizeof(trace_info);
		break;

	case 8:	 //MPI_Irecv
	
	case 10: //MPI_Isend
	
	case 12: //MPI_Recv 
	
	case 15: //MPI_Send

		p2p_ip = (p2p_msg*) buf_ptr;
		printf("id:%ld mpi:%hd comm:%hd src:%hd dest:%hd time:%4.1f size:%d, tag:%hd\n", p2p_ip->id, p2p_ip->mpi_id, p2p_ip->comm_id, p2p_ip->src, p2p_ip->dest, (p2p_ip->exit_time-p2p_ip->entry_time), p2p_ip->length, p2p_ip->tag);
		buf_ptr += sizeof(p2p_msg);
		break;
		
	case 25: //MPI_Wait

		wait_ip = (wait_eve*) buf_ptr;
		printf("id:%ld mpi:%hd p2p:%ld time:%4.1f\n", wait_ip->id, wait_ip->mpi_id, wait_ip->p2p_id, (wait_ip->exit_time-wait_ip->entry_time));
		buf_ptr+= sizeof(wait_eve);
		break;
	case 26: //MPI_Waitall
		waitall_ip = (waitall_eve*) buf_ptr;
		count = waitall_ip -> count;
		printf("id:%ld mpi:%hd count:%d time:%4.1f\n", waitall_ip->id, waitall_ip->mpi_id, waitall_ip ->count, (waitall_ip->exit_time-waitall_ip->entry_time));
		buf_ptr+= sizeof(waitall_eve);
		_uint64_ptr = (_uint64*)buf_ptr;
		printf("MPI_Waitall: ");
		for(i=0; i<count;i++)
			printf("%ld ", _uint64_ptr[i]);
		printf("\n");
		buf_ptr+= count*sizeof(_uint64);
		break;
	case 51:	//MPI_Comm_dup
	case 58:	//MPI_Comm_split
		comm_ip = (comm_eve*) buf_ptr;
		count = comm_ip -> count;
		printf("id:%ld mpi:%hd comm_id:%hd count:%d\n", comm_ip->id, comm_ip->mpi_id, comm_ip ->comm_id, comm_ip->count);
		buf_ptr+= sizeof(comm_eve);
		_uint32_ptr = (_uint32*)buf_ptr;
		//for(i=0; i<count;i++)
		//	printf("%ud ", _uint32_ptr[i]);
		//printf("\n");
		buf_ptr+= count*sizeof(_uint32);
		break;
	
	default:
		printf("id=%d\n", *mpi_id); 
		break;	
	}
	int retlen;	
	mpi_id = (_uint16*) buf_ptr;
	
	if ((buf_ptr-trace_buf == MAXBUF)||(*mpi_id == 0)){
		//printf("read buffer\n");
		memset(trace_buf, 0, MAXBUF);
		retlen = gzread(fp_trace, (char*)trace_buf, MAXBUF);
		if (retlen == -1 ){
			printf("Read trace file: %s error!\n", argv[1]);
			exit(1);
		}else if (retlen == 0 ){
			gzclose(fp_trace);
			return;
		}else if (retlen != MAXBUF){
			printf("Read trace file: %s size error!\n", argv[1]);
			exit(1);
		}
		buf_ptr = trace_buf;
		mpi_id = (_uint16*) buf_ptr;
	}
} 

	gzclose(fp_trace);
}
