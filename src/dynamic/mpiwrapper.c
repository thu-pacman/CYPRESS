/***************************************************************
*                                                                   *
* This library can be used to acquire communication and computation *
* trace for a given MPI program. Written by Jidong Zhai.            *
* July.19.2012. Contact: zhaijidong@gmail.com                       *
* Some of codes use the TAU framwork, copyright of TAU listed here  *
*                                                                   *
********************************************************************/

/****************************************************************************
**			TAU Portable Profiling Package			   **
**			http://www.cs.uoregon.edu/research/tau	           **
*****************************************************************************
**    Copyright 2010  						   	   **
**    Department of Computer and Information Science, University of Oregon **
**    Advanced Computing Laboratory, Los Alamos National Laboratory        **
****************************************************************************/
/****************************************************************************
**	File 		: TauMpi.c      				   **
**	Description 	: TAU Profiling Package				   **
**	Contact		: tau-bugs@cs.uoregon.edu               	   **
**	Documentation	: See http://www.cs.uoregon.edu/research/tau       **
**                                                                         **
**      Description     : MPI Wrapper                                      **
**                                                                         **
****************************************************************************/

#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <string.h>

#include "trace.h"
#include "typedef.h"


#define MAX_REQUESTS  4096

/* Requests */

typedef struct request_list_ {
    MPI_Request request; /* SSS request should be a pointer */
    int         status, size, tag, otherParty;
    int         is_persistent;
    _uint64     id;
    MPI_Comm    comm;
    struct request_list_ *next;
} request_list;

#define RQ_SEND    0x1
#define RQ_RECV    0x2
#define RQ_CANCEL  0x4
/* if MPI_Cancel is called on a request, 'or' RQ_CANCEL into status.
** After a Wait* or Test* is called on that request, check for RQ_CANCEL.
** If the bit is set, check with MPI_Test_cancelled before registering
** the send/receive as 'happening'.
**
*/

#define rq_alloc( head_alloc, newrq ) {\
      if (head_alloc) {\
        newrq=head_alloc;head_alloc=newrq->next;\
	}else{\
      newrq = (request_list*) malloc(sizeof( request_list ));\
      }}

#define rq_remove_at( head, tail, head_alloc, ptr, last ) { \
  if (ptr) { \
    if (!last) { \
      head = ptr->next; \
    } else { \
      last->next = ptr->next; \
      if (tail == ptr) tail = last; \
    } \
	  ptr->next = head_alloc; head_alloc = ptr;}}

#define rq_remove( head, tail, head_alloc, rq ) { \
  request_list *ptr, *last; \
  ptr = head; \
  last = 0; \
  while (ptr && (ptr->request != rq)) { \
    last = ptr; \
    ptr = ptr->next; \
  } \
	rq_remove_at( head, tail, head_alloc, ptr, last );}


#define rq_add( head, tail, rq ) { \
  if (!head) { \
    head = tail = rq; \
  } else { \
    tail->next = rq; tail = rq; \
  }}

#define rq_find( head, req, rq ) { \
  rq = head; \
  while (rq && (rq->request != req)) rq = rq->next; }

#define rq_init( head_alloc ) {\
  int i; request_list *newrq; head_alloc = 0;\
  for (i=0;i<20;i++) {\
      newrq = (request_list*) malloc(sizeof( request_list ));\
      newrq->next = head_alloc;\
      head_alloc = newrq;\
  }}

#define rq_end( head_alloc ) {\
  request_list *rq; while (head_alloc) {\
	rq = head_alloc->next;free(head_alloc);head_alloc=rq;}}

static request_list *requests_head_0=NULL, *requests_tail_0=NULL;

request_list *TauGetRequest( MPI_Request request) {
  request_list *rq;

  rq = requests_head_0;

  while ((rq != NULL) && (rq->request != request)) {
    rq = rq->next;
  }
  return rq;
}

void TauAddRequest (int status, int count, MPI_Datatype datatype, int other, int tag, MPI_Comm comm, MPI_Request *request, int returnVal, int persistent) {

  int typesize;
  request_list *newrq1;

  if (other != MPI_PROC_NULL && returnVal == MPI_SUCCESS) {
    newrq1 = (request_list*) malloc(sizeof( request_list ));
    if (newrq1) {
      PMPI_Type_size( datatype, &typesize );
      newrq1->request = *request;
      newrq1->status = status;
      newrq1->size = typesize * count;
      newrq1->otherParty = other;
      newrq1->id = trace_id;
      newrq1->comm = comm;
      newrq1->tag = tag;
      newrq1->is_persistent = persistent;
      newrq1->next = 0;
      rq_add( requests_head_0, requests_tail_0, newrq1 );
    }
  }
}

/* This routine traverses the list of requests and deletes the given request */
void TauRemoveRequest ( request, note )
MPI_Request request;
char *note;
{
  request_list *rq, *last;

#ifdef DEBUG
  int myrank;
  PMPI_Comm_rank(MPI_COMM_WORLD, &myrank);
#endif /* DEBUG */

  /* look for request */
  rq = requests_head_0;
  last = 0;

  /* first request */
  while ((rq != NULL) && (rq->request != request)) {
#ifdef DEBUG
   printf("Node %d: Comparing %lx %lx\n", myrank, rq->request, request);
#endif /* DEBUG */
    last = rq;
    rq = rq->next;
  }

  if (!rq) {
#ifdef DEBUG
    fprintf( stderr, "Node %d: Request not found in '%s'.\n",myrank, note );
#endif /* DEBUG */
    return ;		/* request not found */
  }
  /* remove the request */
  if (last) {
    if (rq == requests_tail_0) {
      requests_tail_0 = last;
    }
    last->next = rq->next;
  } else {
    requests_head_0 = rq->next;
  }
  free( rq );

  return ;
}

void TauProcessRequest ( request, id, note )
MPI_Request request;
_uint64* id;
char *note;
{
  request_list *rq, *last;
  //int otherid, othertag;

#ifdef DEBUG
  int myrank;
  PMPI_Comm_rank(MPI_COMM_WORLD, &myrank);
#endif /* DEBUG */

  /* look for request */
  rq = requests_head_0;
  last = 0;

  /* first request */
  while ((rq != NULL) && (rq->request != request)) {
#ifdef DEBUG
   printf("Node %d: Comparing %lx %lx\n", myrank, rq->request, request);
#endif /* DEBUG */

    last = rq;
    rq = rq->next;
  }

  if (!rq) {
//#ifdef DEBUG
    fprintf( stderr, "Node %d: Request not found in '%s'.\n", proc_id, note );
    *id = 0;
//#endif /* DEBUG */
    return ;                /* request not found */
  }
#ifdef DEBUG
  else
  {
    printf("Node %d: Request found %lx\n", proc_id, request);
  }
#endif /* DEBUG */

  // Return point-to-point event ID.
  if(rq)
  	*id = rq->id;

  if (rq->is_persistent == 0) {
    /* Remove the record from the request list */
    if (last) {
      if (rq == requests_tail_0) {
	requests_tail_0 = last;
      }
      last->next = rq->next;
    } else {
      requests_head_0 = rq->next;
    }
    free( rq );
  }

  return ;

}


/******************************************************************
*                                                                 *
*                   MPI Functions for Management                  *
*                                                                 *
******************************************************************/

int  MPI_Init( argc, argv )
int * argc;
char *** argv;
{
  int  returnVal;
  int  proc_id;
  int  size;
  //char procname[MPI_MAX_PROCESSOR_NAME];
  //int  procnamelength;

  #ifdef PERF_TRACE
  TRACE_START(0);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Init\n");
  #endif

  returnVal = PMPI_Init( argc, argv );

  #ifdef PERF_TRACE
  requests_head_0 = requests_tail_0 = 0;
  PMPI_Comm_rank( MPI_COMM_WORLD, &proc_id );
  PMPI_Comm_size( MPI_COMM_WORLD, &size );
  TRACE_INIT(proc_id, size);
  TRACE_STOP(0);
  #endif

  return returnVal;
}

#ifdef PERF_MPI_THREADED
int  MPI_Init_thread (argc, argv, required, provided )
int * argc;
char *** argv;
int required;
int *provided;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(1);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Init_thread\n");
  #endif

  returnVal = PMPI_Init_thread( argc, argv, required, provided );

  #ifdef PERF_TRACE
  TRACE_STOP(1);
  #endif

  return returnVal;
}
#endif /* PERF_MPI_THREADED */


int  MPI_Finalize(  )
{
  int  returnVal;
  //int  size;
  //char procname[MPI_MAX_PROCESSOR_NAME];
  //int  procnamelength;

  #ifdef PERF_TRACE
  TRACE_FINISH();
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Finalize\n");
  #endif

  returnVal = PMPI_Finalize();

  return returnVal;
}


/******************************************************************
*                                                                 *
*          MPI Point-to-Point(P2P) Communication Related          *
*                                                                 *
******************************************************************/

int  MPI_Bsend( buf, count, datatype, dest, tag, comm )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
  int returnVal;
  int typesize;

  #ifdef PERF_TRACE
  TRACE_START(3);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Bsend\n");
  #endif

  returnVal = PMPI_Bsend( buf, count, datatype, dest, tag, comm );

  #ifdef PERF_TRACE
  if (dest != MPI_PROC_NULL && returnVal == MPI_SUCCESS) {
    PMPI_Type_size(datatype, &typesize);
    TRACE_P2P(proc_id, dest, tag, comm, typesize*count);
  }
  TRACE_STOP(3);
  #endif

  return returnVal;
}

int  MPI_Bsend_init( buf, count, datatype, dest, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(4);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Bsend_init\n");
  #endif

  returnVal = PMPI_Bsend_init( buf, count, datatype, dest, tag, comm, request );

  #ifdef PERF_TRACE
  TRACE_STOP(4);
  #endif

  return returnVal;
}

int  MPI_Recv_init( buf, count, datatype, source, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int source;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;
  int  typesize;

  #ifdef PERF_TRACE
  TRACE_START(5);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Recv_init\n");
  #endif

  returnVal = PMPI_Recv_init( buf, count, datatype, source, tag, comm, request );

  #ifdef PERF_TRACE
  PMPI_Type_size(datatype, &typesize);
  if (source != MPI_PROC_NULL && returnVal == MPI_SUCCESS) {
    TauAddRequest(RQ_RECV, count, datatype, source, tag, comm, request, returnVal, 1);
    TRACE_P2P(source, proc_id, tag, comm, typesize*count);
  }
  TRACE_STOP(5);
  #endif

  return returnVal;
}

int  MPI_Send_init( buf, count, datatype, dest, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;
  int  typesize;

  #ifdef PERF_TRACE
  TRACE_START(6);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Send_init\n");
  #endif

  returnVal = PMPI_Send_init( buf, count, datatype, dest, tag, comm, request );

  #ifdef PERF_TRACE
  if (dest != MPI_PROC_NULL && returnVal == MPI_SUCCESS) {
    PMPI_Type_size(datatype, &typesize);
    TauAddRequest(RQ_SEND, count, datatype, dest, tag, comm, request, returnVal, 1);
    TRACE_P2P(proc_id, dest, tag, comm, typesize*count);
  }
  TRACE_STOP(6);
  #endif

  return returnVal;
}

int  MPI_Ibsend( buf, count, datatype, dest, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(7);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Ibsend\n");
  #endif

  returnVal = PMPI_Ibsend( buf, count, datatype, dest, tag, comm, request );

  #ifdef PERF_TRACE
  TRACE_STOP(7);
  #endif

  return returnVal;
}

int  MPI_Irecv( buf, count, datatype, source, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int source;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;
  int  typesize;

  #ifdef PERF_TRACE
  TRACE_START(8);
  #endif

  #ifdef DEBUG
  printf("%d:Enter MPI_Irecv\n", proc_id);
  #endif

  returnVal = PMPI_Irecv( buf, count, datatype, source, tag, comm, request );

  #ifdef PERF_TRACE
  PMPI_Type_size( datatype, &typesize );
  if (source != MPI_PROC_NULL && returnVal == MPI_SUCCESS){
    TauAddRequest(RQ_RECV, count, datatype, source, tag, comm, request, returnVal, 0);
    TRACE_P2P(source, proc_id, tag, comm, typesize*count);
  }
  TRACE_STOP(8);
  #endif

  return returnVal;
}

int  MPI_Irsend( buf, count, datatype, dest, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(9);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Irsend\n");
  #endif

  returnVal = PMPI_Irsend( buf, count, datatype, dest, tag, comm, request );

  #ifdef PERF_TRACE
  TRACE_STOP(9);
  #endif

  return returnVal;
}

int  MPI_Isend( buf, count, datatype, dest, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;
  int  typesize;

  #ifdef PERF_TRACE
  TRACE_START(10);
  #endif

  #ifdef DEBUG
  printf("%d:Enter MPI_Isend\n", proc_id);
  #endif

  returnVal = PMPI_Isend( buf, count, datatype, dest, tag, comm, request );

  #ifdef PERF_TRACE
  PMPI_Type_size( datatype, &typesize );
  if (dest != MPI_PROC_NULL && returnVal == MPI_SUCCESS){
    TauAddRequest(RQ_SEND, count, datatype, dest, tag, comm, request, returnVal, 0);
    TRACE_P2P(proc_id, dest, tag, comm, typesize*count);
  }
  TRACE_STOP(10);
  #endif
  return returnVal;
}

int  MPI_Issend( buf, count, datatype, dest, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(11);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Issend\n");
  #endif

  returnVal = PMPI_Issend( buf, count, datatype, dest, tag, comm, request );

  #ifdef PERF_TRACE
  TRACE_STOP(11);
  #endif
  return returnVal;
}

int  MPI_Recv( buf, count, datatype, source, tag, comm, status )
void * buf;
int count;
MPI_Datatype datatype;
int source;
int tag;
MPI_Comm comm;
MPI_Status * status;
{
  MPI_Status local_status;
  int  returnVal;
  int typesize;

  #ifdef PERF_TRACE
  TRACE_START(12);
  if (status == MPI_STATUS_IGNORE) {
    status = &local_status;
  }
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Recv\n");
  #endif

  returnVal = PMPI_Recv( buf, count, datatype, source, tag, comm, status );

  #ifdef PERF_TRACE
  PMPI_Type_size( datatype, &typesize );
  if (source != MPI_PROC_NULL && returnVal == MPI_SUCCESS) {
    TRACE_P2P(status->MPI_SOURCE, proc_id, status->MPI_TAG, comm, typesize*count);
  }
  TRACE_STOP(12);
  #endif

  return returnVal;
}

int  MPI_Rsend( buf, count, datatype, dest, tag, comm )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(13);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Rsend\n");
  #endif

  returnVal = PMPI_Rsend( buf, count, datatype, dest, tag, comm );

  #ifdef PERF_TRACE
  TRACE_STOP(13);
  #endif

  return returnVal;
}

int  MPI_Rsend_init( buf, count, datatype, dest, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(14);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Rsend_init\n");
  #endif

  returnVal = PMPI_Rsend_init( buf, count, datatype, dest, tag, comm, request );

  #ifdef PERF_TRACE
  TRACE_STOP(14);
  #endif

  return returnVal;
}


int  MPI_Send( buf, count, datatype, dest, tag, comm )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
  int  returnVal;
  int  typesize;

  #ifdef PERF_TRACE
  TRACE_START(15);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Send\n");
  #endif

  returnVal = PMPI_Send( buf, count, datatype, dest, tag, comm );

  #ifdef PERF_TRACE
  if (dest != MPI_PROC_NULL && returnVal == MPI_SUCCESS) {
    PMPI_Type_size( datatype, &typesize );
    TRACE_P2P(proc_id, dest, tag, comm, typesize*count);
  }
  TRACE_STOP(15);
  #endif

  return returnVal;
}

int  MPI_Sendrecv( sendbuf, sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status )
void * sendbuf;
int sendcount;
MPI_Datatype sendtype;
int dest;
int sendtag;
void * recvbuf;
int recvcount;
MPI_Datatype recvtype;
int source;
int recvtag;
MPI_Comm comm;
MPI_Status * status;
{
  int  returnVal;
  int  typesize;
  _uint64 buf[2];

  #ifdef PERF_TRACE
  //TRACE_START(16);
  //Use MPI_Isend
  TRACE_START(10);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Sendrecv\n");
  #endif

  returnVal = PMPI_Sendrecv( sendbuf, sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status );

  #ifdef PERF_TRACE
  PMPI_Type_size(sendtype, &typesize);
  if (dest != MPI_PROC_NULL && returnVal == MPI_SUCCESS){
    buf[0]= trace_id;
	TRACE_P2P(proc_id, dest, sendtag, comm, typesize*sendcount);
  }
  TRACE_STOP(10);

  TRACE_START(8);
  PMPI_Type_size(recvtype, &typesize);
  if (source != MPI_PROC_NULL && returnVal == MPI_SUCCESS){
    buf[1]= trace_id;
	TRACE_P2P(source, proc_id, recvtag, comm, typesize*recvcount);
  }
  TRACE_STOP(8);

  //MPI_Waitall
  TRACE_START(26);
  TRACE_WAITALL(2, buf);
  TRACE_STOP(26);

  //TRACE_STOP(16);
  #endif

  return returnVal;
}

int  MPI_Sendrecv_replace( buf, count, datatype, dest, sendtag, source, recvtag, comm, status )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int sendtag;
int source;
int recvtag;
MPI_Comm comm;
MPI_Status * status;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(17);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Sendrecv_replace\n");
  #endif

  returnVal = PMPI_Sendrecv_replace( buf, count, datatype, dest, sendtag, source, recvtag, comm, status );

  #ifdef PERF_TRACE
  TRACE_STOP(17);
  #endif

  return returnVal;
}

int  MPI_Ssend( buf, count, datatype, dest, tag, comm )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(18);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Ssend\n");
  #endif

  returnVal = PMPI_Ssend( buf, count, datatype, dest, tag, comm );

  #ifdef PERF_TRACE
  TRACE_STOP(18);
  #endif

  return returnVal;
}

int  MPI_Ssend_init( buf, count, datatype, dest, tag, comm, request )
void * buf;
int count;
MPI_Datatype datatype;
int dest;
int tag;
MPI_Comm comm;
MPI_Request * request;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(19);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Ssend_init\n");
  #endif

  returnVal = PMPI_Ssend_init( buf, count, datatype, dest, tag, comm, request );

  #ifdef PERF_TRACE
  TRACE_STOP(19);
  #endif

  return returnVal;
}


int   MPI_Test( request, flag, status )
MPI_Request * request;
int * flag;
MPI_Status * status;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(20);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Test\n");
  #endif

  returnVal = PMPI_Test( request, flag, status );

  #ifdef PERF_TRACE
  TRACE_STOP(20);
  #endif

  return returnVal;
}

int  MPI_Testall( count, array_of_requests, flag, array_of_statuses )
int count;
MPI_Request * array_of_requests;
int * flag;
MPI_Status * array_of_statuses;
{
  int returnVal;

  #ifdef PERF_TRACE
  TRACE_START(21);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Testall\n");
  #endif

  returnVal = PMPI_Testall( count, array_of_requests, flag, array_of_statuses );

  #ifdef PERF_TRACE
  TRACE_STOP(21);
  #endif

  return returnVal;
}

int  MPI_Testany( count, array_of_requests, index, flag, status )
int count;
MPI_Request * array_of_requests;
int * index;
int * flag;
MPI_Status * status;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(22);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Testany\n");
  #endif

  returnVal = PMPI_Testany( count, array_of_requests, index, flag, status );

  #ifdef PERF_TRACE
  TRACE_STOP(22);
  #endif

  return returnVal;
}

int  MPI_Test_cancelled( status, flag )
MPI_Status * status;
int * flag;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(23);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Test_cancelled\n");
  #endif

  returnVal = PMPI_Test_cancelled( status, flag );

  #ifdef PERF_TRACE
  TRACE_STOP(23);
  #endif

  return returnVal;
}

int  MPI_Testsome( incount, array_of_requests, outcount, array_of_indices, array_of_statuses )
int incount;
MPI_Request * array_of_requests;
int * outcount;
int * array_of_indices;
MPI_Status * array_of_statuses;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(24);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Testsome\n");
  #endif

  returnVal = PMPI_Testsome( incount, array_of_requests, outcount, array_of_indices, array_of_statuses );

  #ifdef PERF_TRACE
  TRACE_STOP(24);
  #endif

  return returnVal;
}

int   MPI_Wait( request, status )
MPI_Request * request;
MPI_Status * status;
{
  int   returnVal;
  MPI_Status local_status;
  MPI_Request saverequest;
  _uint64 id;

  #ifdef PERF_TRACE
  TRACE_START(25);
  if (status == MPI_STATUS_IGNORE) {
    status = &local_status;
  }
  saverequest = *request;
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Wait\n");
  #endif

  returnVal = PMPI_Wait( request, status );

  #ifdef PERF_TRACE
  TauProcessRequest(saverequest, &id, "MPI_Wait");
  TRACE_WAIT(id);
  TRACE_STOP(25);
  #endif

  return returnVal;
}

int  MPI_Waitall( count, array_of_requests, array_of_statuses )
int count;
MPI_Request * array_of_requests;
MPI_Status * array_of_statuses;
{
  int  returnVal, i;
  MPI_Request saverequest[MAX_REQUESTS];
  _uint64* buf;
  _uint64 id;

  #ifdef PERF_TRACE
  TRACE_START(26);
  for (i = 0; i < count; i++) {
    saverequest[i] = array_of_requests[i];
  }
  #endif

  #ifdef DEBUG
  printf("%d:Enter MPI_Waitall\n", proc_id);
  #endif

  returnVal = PMPI_Waitall( count, array_of_requests, array_of_statuses );

  #ifdef PERF_TRACE
  buf = (_uint64*) malloc(count*sizeof(_uint64));
  for(i=0; i < count; i++) {
    TauProcessRequest(saverequest[i], &id, "MPI_Waitall");
    buf[i] = id;
  }
  TRACE_WAITALL(count, buf);
  free(buf);
  TRACE_STOP(26);
  #endif

  return returnVal;
}

int  MPI_Waitany( count, array_of_requests, index, status )
int count;
MPI_Request * array_of_requests;
int * index;
MPI_Status * status;
{
  int  returnVal;
  MPI_Request saverequest;
  _uint64 id;

  #ifdef PERF_TRACE
  TRACE_START(27);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Waitany\n");
  #endif

  returnVal = PMPI_Waitany( count, array_of_requests, index, status );

  #ifdef PERF_TRACE
  saverequest = array_of_requests[*index];
  TauProcessRequest(saverequest, &id, "MPI_Waitany");
  TRACE_WAIT(id);
  TRACE_STOP(27);
  #endif

  return returnVal;
}

int  MPI_Waitsome( incount, array_of_requests, outcount, array_of_indices, array_of_statuses )
int incount;
MPI_Request * array_of_requests;
int * outcount;
int * array_of_indices;
MPI_Status * array_of_statuses;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(28);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Waitsome\n");
  #endif

  returnVal = PMPI_Waitsome( incount, array_of_requests, outcount, array_of_indices, array_of_statuses );

  #ifdef PERF_TRACE
  TRACE_STOP(28);
  #endif

  return returnVal;
}

int  MPI_Cancel( request )
MPI_Request * request;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(29);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Cancel\n");
  #endif

  returnVal = PMPI_Cancel( request );

  #ifdef PERF_TRACE
  TRACE_STOP(29);
  #endif

  return returnVal;
}

int  MPI_Request_free( request )
MPI_Request * request;
{
  int  returnVal;
  MPI_Request saverequest;

  saverequest=*request;

  #ifdef PERF_TRACE
  TRACE_START(30);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Request_free\n");
  #endif

  returnVal = PMPI_Request_free( request );

  #ifdef PERF_TRACE
  TauRemoveRequest (saverequest, "MPI_Request_free");
  TRACE_STOP(30);
  #endif

  return returnVal;
}

int  MPI_Start( request )
MPI_Request * request;
{
  int  returnVal;
  _uint64 id;
  MPI_Request saverequest;

  #ifdef PERF_TRACE
  TRACE_START(31);
  saverequest = *request;
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Start\n");
  #endif

  returnVal = PMPI_Start( request );

  #ifdef PERF_TRACE
  TauProcessRequest(saverequest, &id, "MPI_Start");
  TRACE_WAIT(id);
  TRACE_STOP(31);
  #endif

  return returnVal;
}

int  MPI_Startall( count, array_of_requests )
int count;
MPI_Request * array_of_requests;
{
  int  returnVal, i;
  MPI_Request saverequest[MAX_REQUESTS];
  _uint64* buf;
  _uint64 id;

  #ifdef PERF_TRACE
  TRACE_START(32);
  for (i = 0; i < count; i++) {
    saverequest[i] = array_of_requests[i];
  }
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Startall\n");
  #endif

  returnVal = PMPI_Startall( count, array_of_requests );

  #ifdef PERF_TRACE
  buf = (_uint64*) malloc(count*sizeof(_uint64));
  for(i=0; i < count; i++) {
    TauProcessRequest(saverequest[i], &id, "MPI_Startall");
    buf[i] = id;
  }
  TRACE_WAITALL(count, buf);
  free(buf);
  TRACE_STOP(32);
  #endif

  return returnVal;
}

int  MPI_Iprobe( source, tag, comm, flag, status )
int source;
int tag;
MPI_Comm comm;
int * flag;
MPI_Status * status;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(33);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Iprobe\n");
  #endif

  returnVal = PMPI_Iprobe( source, tag, comm, flag, status );

  #ifdef PERF_TRACE
  TRACE_STOP(33);
  #endif

  return returnVal;
}

int  MPI_Probe( source, tag, comm, status )
int source;
int tag;
MPI_Comm comm;
MPI_Status * status;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(34);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Probe\n");
  #endif

  returnVal = PMPI_Probe( source, tag, comm, status );

  #ifdef PERF_TRACE
  TRACE_STOP(34);
  #endif

  return returnVal;
}

/******************************************************************
*                                                                 *
*               MPI Collective Communication Related              *
*                                                                 *
******************************************************************/

int   MPI_Allgather( sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm )
void * sendbuf;
int sendcount;
MPI_Datatype sendtype;
void * recvbuf;
int recvcount;
MPI_Datatype recvtype;
MPI_Comm comm;
{
  int   returnVal;
  int	typesize_s;
  int	typesize_r;

  #ifdef PERF_TRACE
  TRACE_START(35);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Allgather\n");
  #endif

  returnVal = PMPI_Allgather( sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size(sendtype, &typesize_s);
  PMPI_Type_size(recvtype, &typesize_r);
  TRACE_COLL(typesize_s*sendcount, typesize_r*recvcount, comm, -1);
  TRACE_STOP(35);
  #endif

  return returnVal;
}

int   MPI_Allgatherv( sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, comm )
void * sendbuf;
int sendcount;
MPI_Datatype sendtype;
void * recvbuf;
int * recvcounts;
int * displs;
MPI_Datatype recvtype;
MPI_Comm comm;
{
  int   returnVal;
  int   typesize_s;
  //int   typesize_r;

  #ifdef PERF_TRACE
  TRACE_START(36);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Allgatherv\n");
  #endif

  returnVal = PMPI_Allgatherv( sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size(sendtype, &typesize_s);
  TRACE_COLL(typesize_s*sendcount, -1, comm, -1);
  TRACE_STOP(36);
  #endif

  return returnVal;
}

int   MPI_Allreduce( sendbuf, recvbuf, count, datatype, op, comm )
void * sendbuf;
void * recvbuf;
int count;
MPI_Datatype datatype;
MPI_Op op;
MPI_Comm comm;
{
  int   returnVal;
  int	typesize;

  #ifdef PERF_TRACE
  TRACE_START(37);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Allreduce\n");
  #endif

  returnVal = PMPI_Allreduce( sendbuf, recvbuf, count, datatype, op, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size(datatype, &typesize);
  TRACE_COLL(typesize*count, -1, comm, -1);
  TRACE_STOP(37);
  #endif

  return returnVal;
}

int  MPI_Alltoall( sendbuf, sendcount, sendtype, recvbuf, recvcnt, recvtype, comm )
void * sendbuf;
int sendcount;
MPI_Datatype sendtype;
void * recvbuf;
int recvcnt;
MPI_Datatype recvtype;
MPI_Comm comm;
{
  int  returnVal;
  int  typesize_s;
  int  typesize_r;

  #ifdef PERF_TRACE
  TRACE_START(38);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Alltoall\n");
  #endif

  returnVal = PMPI_Alltoall( sendbuf, sendcount, sendtype, recvbuf, recvcnt, recvtype, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size(sendtype, &typesize_s);
  PMPI_Type_size(recvtype, &typesize_r);
  TRACE_COLL(typesize_s*sendcount, typesize_r*recvcnt, comm, -1);
  TRACE_STOP(38);
  #endif

  return returnVal;
}

int   MPI_Alltoallv( sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm )
void * sendbuf;
int * sendcnts;
int * sdispls;
MPI_Datatype sendtype;
void * recvbuf;
int * recvcnts;
int * rdispls;
MPI_Datatype recvtype;
MPI_Comm comm;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(39);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Alltoallv\n");
  #endif

  //_TO_DO: Add support for MPI_Alltoallv

  returnVal = PMPI_Alltoallv( sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm );

  #ifdef PERF_TRACE
  TRACE_STOP(39);
  #endif

  return returnVal;
}

int   MPI_Barrier( comm )
MPI_Comm comm;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(40);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Barrier\n");
  #endif

  returnVal = PMPI_Barrier( comm );

  #ifdef PERF_TRACE
  TRACE_COLL(-1, -1, comm, -1);
  TRACE_STOP(40);
  #endif

  return returnVal;
}

int   MPI_Bcast( buffer, count, datatype, root, comm )
void * buffer;
int count;
MPI_Datatype datatype;
int root;
MPI_Comm comm;
{
  int   returnVal;
  int   typesize;

  #ifdef PERF_TRACE
  TRACE_START(41);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Bcast\n");
  #endif

  returnVal = PMPI_Bcast( buffer, count, datatype, root, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size( datatype, &typesize );
  TRACE_COLL(typesize*count, -1, comm, root);
  TRACE_STOP(41);
  #endif

  return returnVal;
}

int   MPI_Gather( sendbuf, sendcnt, sendtype, recvbuf, recvcount, recvtype, root, comm )
void * sendbuf;
int sendcnt;
MPI_Datatype sendtype;
void * recvbuf;
int recvcount;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
  int   returnVal;
  int  typesize_s;

  #ifdef PERF_TRACE
  TRACE_START(42);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Gather\n");
  #endif

  returnVal = PMPI_Gather( sendbuf, sendcnt, sendtype, recvbuf, recvcount, recvtype, root, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size(sendtype, &typesize_s);
  TRACE_COLL(typesize_s*sendcnt, -1, comm, root);
  TRACE_STOP(42);
  #endif

  return returnVal;
}

int   MPI_Gatherv( sendbuf, sendcnt, sendtype, recvbuf, recvcnts, displs, recvtype, root, comm )
void * sendbuf;
int sendcnt;
MPI_Datatype sendtype;
void * recvbuf;
int * recvcnts;
int * displs;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
  int   returnVal;
  int   typesize_s;
  //int   typesize_r;

  #ifdef PERF_TRACE
  TRACE_START(43);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Gatherv\n");
  #endif

  returnVal = PMPI_Gatherv( sendbuf, sendcnt, sendtype, recvbuf, recvcnts, displs, recvtype, root, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size(sendtype, &typesize_s);
  TRACE_COLL(typesize_s*sendcnt, -1, comm, root);
  TRACE_STOP(43);
  #endif

  return returnVal;
}

int   MPI_Reduce_scatter( sendbuf, recvbuf, recvcnts, datatype, op, comm )
void * sendbuf;
void * recvbuf;
int * recvcnts;
MPI_Datatype datatype;
MPI_Op op;
MPI_Comm comm;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(44);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Reduce_scatter\n");
  #endif

  returnVal = PMPI_Reduce_scatter( sendbuf, recvbuf, recvcnts, datatype, op, comm );

  #ifdef PERF_TRACE
  TRACE_STOP(44);
  #endif

  return returnVal;
}

int   MPI_Reduce( sendbuf, recvbuf, count, datatype, op, root, comm )
void * sendbuf;
void * recvbuf;
int count;
MPI_Datatype datatype;
MPI_Op op;
int root;
MPI_Comm comm;
{
  int   returnVal;
  int	typesize;

  #ifdef PERF_TRACE
  TRACE_START(45);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Reduce\n");
  #endif

  returnVal = PMPI_Reduce( sendbuf, recvbuf, count, datatype, op, root, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size( datatype, &typesize );
  TRACE_COLL(typesize*count, -1, comm, root);
  TRACE_STOP(45);
  #endif

  return returnVal;
}

int   MPI_Scan( sendbuf, recvbuf, count, datatype, op, comm )
void * sendbuf;
void * recvbuf;
int count;
MPI_Datatype datatype;
MPI_Op op;
MPI_Comm comm;
{
  int   returnVal;
  int	typesize;

  #ifdef PERF_TRACE
  TRACE_START(46);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Scan\n");
  #endif

  returnVal = PMPI_Scan( sendbuf, recvbuf, count, datatype, op, comm );

  #ifdef PERF_TRACE
  PMPI_Type_size( datatype, &typesize );
  TRACE_COLL(typesize*count, -1, comm, -1);
  TRACE_STOP(46);
  #endif

  return returnVal;
}

int   MPI_Scatter( sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm )
void * sendbuf;
int sendcnt;
MPI_Datatype sendtype;
void * recvbuf;
int recvcnt;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(47);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Scatter\n");
  #endif

  returnVal = PMPI_Scatter( sendbuf, sendcnt, sendtype, recvbuf, recvcnt, recvtype, root, comm );

  #ifdef PERF_TRACE
  TRACE_STOP(47);
  #endif

  return returnVal;
}

int   MPI_Scatterv( sendbuf, sendcnts, displs, sendtype, recvbuf, recvcnt, recvtype, root, comm )
void * sendbuf;
int * sendcnts;
int * displs;
MPI_Datatype sendtype;
void * recvbuf;
int recvcnt;
MPI_Datatype recvtype;
int root;
MPI_Comm comm;
{
  int returnVal;

  #ifdef PERF_TRACE
  TRACE_START(48);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Scatterv\n");
  #endif

  returnVal = PMPI_Scatterv( sendbuf, sendcnts, displs, sendtype, recvbuf, recvcnt, recvtype, root, comm );

  #ifdef PERF_TRACE
  TRACE_STOP(48);
  #endif

  return returnVal;
}

/******************************************************************
*                                                                 *
*               MPI Communicator Related Functions                *
*                                                                 *
******************************************************************/

int   MPI_Comm_compare( comm1, comm2, result )
MPI_Comm comm1;
MPI_Comm comm2;
int * result;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(49);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Comm_compare\n");
  #endif

  returnVal = PMPI_Comm_compare( comm1, comm2, result );

  #ifdef PERF_TRACE
  TRACE_STOP(49);
  #endif

  return returnVal;
}

int   MPI_Comm_create( comm, group, comm_out )
MPI_Comm comm;
MPI_Group group;
MPI_Comm * comm_out;
{
  int   returnVal;
  MPI_Group group1, group2;
  int newsize, i;
  int* rank1;
  int* rank2;

  #ifdef PERF_TRACE
  TRACE_START(50);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Comm_create\n");
  #endif

  returnVal = PMPI_Comm_create( comm, group, comm_out );

  #ifdef PERF_TRACE
  if (*comm_out == MPI_COMM_NULL){
  	printf("NULL \n");
    TRACE_STOP(50);
	return returnVal;
  }

  PMPI_Comm_group(*comm_out, &group1);
  PMPI_Comm_group(MPI_COMM_WORLD, &group2);
  PMPI_Comm_size(*comm_out, &newsize);

  rank1 = (int*)malloc(newsize*sizeof(int));
  rank2 = (int*)malloc(newsize*sizeof(int));
  memset(rank1, 0, newsize*sizeof(int));
  memset(rank2, 0, newsize*sizeof(int));

  if ((rank1==NULL)||(rank2==NULL)){
  	printf("MPI_Comm_create: malloc error!\n");
	exit(1);
  }

  for(i=0; i<newsize; i++)
    rank1[i]=i;

  PMPI_Group_translate_ranks(group1, newsize, rank1, group2, rank2);
  TRACE_COMM(*comm_out, newsize, rank2);
  free(rank1);
  free(rank2);
  TRACE_STOP(50);
  #endif

  return returnVal;
}

int   MPI_Comm_dup( comm, comm_out )
MPI_Comm comm;
MPI_Comm * comm_out;
{
  int   returnVal;
  MPI_Group group1, group2;
  int newsize, i;
  int* rank1;
  int* rank2;

  #ifdef PERF_TRACE
  TRACE_START(51);
  #endif

  #ifdef DEBUG
  printf("%d:Enter MPI_Comm_dup\n", proc_id);
  #endif

  returnVal = PMPI_Comm_dup( comm, comm_out );

  #ifdef PERF_TRACE
  if (*comm_out == MPI_COMM_NULL){
  	printf("NULL \n");
        TRACE_STOP(51);
	return returnVal;
  }

  PMPI_Comm_group(*comm_out, &group1);
  PMPI_Comm_group(MPI_COMM_WORLD, &group2);
  PMPI_Comm_size(*comm_out, &newsize);

  rank1 = (int*)malloc(newsize*sizeof(int));
  rank2 = (int*)malloc(newsize*sizeof(int));
  memset(rank1, 0, newsize*sizeof(int));
  memset(rank2, 0, newsize*sizeof(int));

  if ((rank1==NULL)||(rank2==NULL)){
  	printf("MPI_Comm_dup: malloc error!\n");
	exit(1);
  }

  for(i=0; i<newsize; i++)
    rank1[i]=i;

  PMPI_Group_translate_ranks(group1, newsize, rank1, group2, rank2);
  TRACE_COMM(*comm_out, newsize, rank2);
  free(rank1);
  free(rank2);
  TRACE_STOP(51);
  #endif

  return returnVal;
}

int   MPI_Comm_free( comm )
MPI_Comm * comm;
{
  int   returnVal;
  int* rank2=NULL;

  #ifdef PERF_TRACE
  TRACE_START(52);
  #endif

  #ifdef DEBUG
  printf("Enter MPI_Comm_free\n");
  #endif

  returnVal = PMPI_Comm_free( comm );

  #ifdef PERF_TRACE
  TRACE_COMM(*comm, 0, rank2);
  TRACE_STOP(52);
  #endif

  return returnVal;
}

int   MPI_Comm_group( comm, group )
MPI_Comm comm;
MPI_Group * group;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(53);
  #endif

  returnVal = PMPI_Comm_group( comm, group );

  #ifdef PERF_TRACE
  TRACE_STOP(53);
  #endif

  return returnVal;
}

int   MPI_Comm_rank( comm, rank )
MPI_Comm comm;
int * rank;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(54);
  #endif

  returnVal = PMPI_Comm_rank( comm, rank );

  #ifdef PERF_TRACE
  TRACE_STOP(54);
  #endif

  return returnVal;
}

int   MPI_Comm_remote_group( comm, group )
MPI_Comm comm;
MPI_Group * group;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(55);
  #endif

  returnVal = PMPI_Comm_remote_group( comm, group );

  #ifdef PERF_TRACE
  TRACE_STOP(55);
  #endif

  return returnVal;
}

int   MPI_Comm_remote_size( comm, size )
MPI_Comm comm;
int * size;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(56);
  #endif

  returnVal = PMPI_Comm_remote_size( comm, size );

  #ifdef PERF_TRACE
  TRACE_STOP(56);
  #endif

  return returnVal;
}

int   MPI_Comm_size( comm, size )
MPI_Comm comm;
int * size;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(57);
  #endif

  returnVal = PMPI_Comm_size( comm, size );

  #ifdef PERF_TRACE
  TRACE_STOP(57);
  #endif

  return returnVal;
}

int   MPI_Comm_split( comm, color, key, comm_out )
MPI_Comm comm;
int color;
int key;
MPI_Comm * comm_out;
{
  int   returnVal;
  MPI_Group group1, group2;
  int newsize, i;
  int* rank1;
  int* rank2;

  #ifdef PERF_TRACE
  TRACE_START(58);
  #endif

  returnVal = PMPI_Comm_split( comm, color, key, comm_out );

  #ifdef PERF_TRACE
  if (*comm_out == MPI_COMM_NULL){
  	printf("NULL \n");
        TRACE_STOP(58);
	return returnVal;
  }

  PMPI_Comm_group(*comm_out, &group1);
  PMPI_Comm_group(MPI_COMM_WORLD, &group2);
  PMPI_Comm_size(*comm_out, &newsize);

  rank1 = (int*)malloc(newsize*sizeof(int));
  rank2 = (int*)malloc(newsize*sizeof(int));
  memset(rank1, 0, newsize*sizeof(int));
  memset(rank2, 0, newsize*sizeof(int));

  if ((rank1==NULL)||(rank2==NULL)){
  	printf("MPI_Comm_split: malloc error!\n");
	exit(1);
  }

  for(i=0; i<newsize; i++)
  rank1[i]=i;

  PMPI_Group_translate_ranks(group1, newsize, rank1, group2, rank2);
  TRACE_COMM(*comm_out, newsize, rank2);

  free(rank1);
  free(rank2);

  TRACE_STOP(58);
  #endif

  return returnVal;
}

int   MPI_Comm_test_inter( comm, flag )
MPI_Comm comm;
int * flag;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(59);
  #endif

  returnVal = PMPI_Comm_test_inter( comm, flag );

  #ifdef PERF_TRACE
  TRACE_STOP(59);
  #endif

  return returnVal;
}

int   MPI_Group_compare( group1, group2, result )
MPI_Group group1;
MPI_Group group2;
int * result;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(60);
  #endif

  returnVal = PMPI_Group_compare( group1, group2, result );

  #ifdef PERF_TRACE
  TRACE_STOP(60);
  #endif

  return returnVal;
}

int   MPI_Group_difference( group1, group2, group_out )
MPI_Group group1;
MPI_Group group2;
MPI_Group * group_out;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(61);
  #endif

  returnVal = PMPI_Group_difference( group1, group2, group_out );

  #ifdef PERF_TRACE
  TRACE_STOP(61);
  #endif

  return returnVal;
}

int   MPI_Group_excl( group, n, ranks, newgroup )
MPI_Group group;
int n;
int * ranks;
MPI_Group * newgroup;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(62);
  #endif

  returnVal = PMPI_Group_excl( group, n, ranks, newgroup );

  #ifdef PERF_TRACE
  TRACE_STOP(62);
  #endif

  return returnVal;
}

int   MPI_Group_free( group )
MPI_Group * group;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(63);
  #endif

  returnVal = PMPI_Group_free( group );

  #ifdef PERF_TRACE
  TRACE_STOP(63);
  #endif

  return returnVal;
}

int   MPI_Group_incl( group, n, ranks, group_out )
MPI_Group group;
int n;
int * ranks;
MPI_Group * group_out;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(64);
  #endif

  returnVal = PMPI_Group_incl( group, n, ranks, group_out );

  #ifdef PERF_TRACE
  TRACE_STOP(64);
  #endif

  return returnVal;
}

int   MPI_Group_intersection( group1, group2, group_out )
MPI_Group group1;
MPI_Group group2;
MPI_Group * group_out;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(65);
  #endif

  returnVal = PMPI_Group_intersection( group1, group2, group_out );

  #ifdef PERF_TRACE
  TRACE_STOP(65);
  #endif

  return returnVal;
}

int   MPI_Group_rank( group, rank )
MPI_Group group;
int * rank;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(66);
  #endif

  returnVal = PMPI_Group_rank( group, rank );

  #ifdef PERF_TRACE
  TRACE_STOP(66);
  #endif

  return returnVal;
}

int   MPI_Group_range_excl( group, n, ranges, newgroup )
MPI_Group group;
int n;
int ranges[][3];
MPI_Group * newgroup;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(67);
  #endif

  returnVal = PMPI_Group_range_excl( group, n, ranges, newgroup );

  #ifdef PERF_TRACE
  TRACE_STOP(67);
  #endif

  return returnVal;
}

int   MPI_Group_range_incl( group, n, ranges, newgroup )
MPI_Group group;
int n;
int ranges[][3];
MPI_Group * newgroup;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(68);
  #endif

  returnVal = PMPI_Group_range_incl( group, n, ranges, newgroup );

  #ifdef PERF_TRACE
  TRACE_STOP(68);
  #endif

  return returnVal;
}

int   MPI_Group_size( group, size )
MPI_Group group;
int * size;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(69);
  #endif

  returnVal = PMPI_Group_size( group, size );

  #ifdef PERF_TRACE
  TRACE_STOP(69);
  #endif

  return returnVal;
}

int   MPI_Group_translate_ranks( group_a, n, ranks_a, group_b, ranks_b )
MPI_Group group_a;
int n;
int * ranks_a;
MPI_Group group_b;
int * ranks_b;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(70);
  #endif

  returnVal = PMPI_Group_translate_ranks( group_a, n, ranks_a, group_b, ranks_b );

  #ifdef PERF_TRACE
  TRACE_STOP(70);
  #endif

  return returnVal;
}

int   MPI_Group_union( group1, group2, group_out )
MPI_Group group1;
MPI_Group group2;
MPI_Group * group_out;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(71);
  #endif

  returnVal = PMPI_Group_union( group1, group2, group_out );

  #ifdef PERF_TRACE
  TRACE_STOP(71);
  #endif

  return returnVal;
}

int   MPI_Intercomm_create( local_comm, local_leader, peer_comm, remote_leader, tag, comm_out )
MPI_Comm local_comm;
int local_leader;
MPI_Comm peer_comm;
int remote_leader;
int tag;
MPI_Comm * comm_out;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(72);
  #endif

  returnVal = PMPI_Intercomm_create( local_comm, local_leader, peer_comm, remote_leader, tag, comm_out );

  #ifdef PERF_TRACE
  TRACE_STOP(72);
  #endif

  return returnVal;
}

int   MPI_Intercomm_merge( comm, high, comm_out )
MPI_Comm comm;
int high;
MPI_Comm * comm_out;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(73);
  #endif

  returnVal = PMPI_Intercomm_merge( comm, high, comm_out );

  #ifdef PERF_TRACE
  TRACE_STOP(73);
  #endif

  return returnVal;
}

int   MPI_Keyval_create( copy_fn, delete_fn, keyval, extra_state )
MPI_Copy_function * copy_fn;
MPI_Delete_function * delete_fn;
int * keyval;
void * extra_state;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(74);
  #endif

  returnVal = PMPI_Keyval_create( copy_fn, delete_fn, keyval, extra_state );

  #ifdef PERF_TRACE
  TRACE_STOP(74);
  #endif

  return returnVal;
}

int   MPI_Keyval_free( keyval )
int * keyval;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(75);
  #endif

  returnVal = PMPI_Keyval_free( keyval );

  #ifdef PERF_TRACE
  TRACE_STOP(75);
  #endif

  return returnVal;
}

int   MPI_Cart_coords( comm, rank, maxdims, coords )
MPI_Comm comm;
int rank;
int maxdims;
int * coords;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(76);
  #endif

  returnVal = PMPI_Cart_coords( comm, rank, maxdims, coords );

  #ifdef PERF_TRACE
  TRACE_STOP(76);
  #endif

  return returnVal;
}

int   MPI_Cart_create( comm_old, ndims, dims, periods, reorder, comm_cart )
MPI_Comm comm_old;
int ndims;
int * dims;
int * periods;
int reorder;
MPI_Comm * comm_cart;
{
  int   returnVal;
  MPI_Group group1, group2;
  int newsize, i;
  int* rank1;
  int* rank2;

  #ifdef PERF_TRACE
  TRACE_START(77);
  #endif

  returnVal = PMPI_Cart_create( comm_old, ndims, dims, periods, reorder, comm_cart );

  #ifdef PERF_TRACE
  if (*comm_cart == MPI_COMM_NULL){
  	printf("NULL \n");
    TRACE_STOP(77);
	return returnVal;
  }

  PMPI_Comm_group(*comm_cart, &group1);
  PMPI_Comm_group(MPI_COMM_WORLD, &group2);
  PMPI_Comm_size(*comm_cart, &newsize);

  rank1 = (int*)malloc(newsize*sizeof(int));
  rank2 = (int*)malloc(newsize*sizeof(int));
  memset(rank1, 0, newsize*sizeof(int));
  memset(rank2, 0, newsize*sizeof(int));

  if ((rank1==NULL)||(rank2==NULL)){
  	printf("MPI_Cart_create: malloc error!\n");
	exit(1);
  }

  for(i=0; i<newsize; i++)
    rank1[i]=i;

  PMPI_Group_translate_ranks(group1, newsize, rank1, group2, rank2);
  TRACE_COMM(*comm_cart, newsize, rank2);
  free(rank1);
  free(rank2);
  TRACE_STOP(77);
  #endif

  return returnVal;
}

int   MPI_Cart_get( comm, maxdims, dims, periods, coords )
MPI_Comm comm;
int maxdims;
int * dims;
int * periods;
int * coords;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(78);
  #endif

  returnVal = PMPI_Cart_get( comm, maxdims, dims, periods, coords );

  #ifdef PERF_TRACE
  TRACE_STOP(78);
  #endif

  return returnVal;
}

int   MPI_Cart_map( comm_old, ndims, dims, periods, newrank )
MPI_Comm comm_old;
int ndims;
int * dims;
int * periods;
int * newrank;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(79);
  #endif

  returnVal = PMPI_Cart_map( comm_old, ndims, dims, periods, newrank );

  #ifdef PERF_TRACE
  TRACE_STOP(79);
  #endif

  return returnVal;
}

int   MPI_Cart_rank( comm, coords, rank )
MPI_Comm comm;
int * coords;
int * rank;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(80);
  #endif

  returnVal = PMPI_Cart_rank( comm, coords, rank );

  #ifdef PERF_TRACE
  TRACE_STOP(80);
  #endif

  return returnVal;
}

int   MPI_Cart_shift( comm, direction, displ, source, dest )
MPI_Comm comm;
int direction;
int displ;
int * source;
int * dest;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(81);
  #endif

  returnVal = PMPI_Cart_shift( comm, direction, displ, source, dest );

  #ifdef PERF_TRACE
  TRACE_STOP(81);
  #endif

  return returnVal;
}

int   MPI_Cart_sub( comm, remain_dims, comm_new )
MPI_Comm comm;
int * remain_dims;
MPI_Comm * comm_new;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(82);
  #endif

  returnVal = PMPI_Cart_sub( comm, remain_dims, comm_new );

  #ifdef PERF_TRACE
  TRACE_STOP(82);
  #endif

  return returnVal;
}

int   MPI_Cartdim_get( comm, ndims )
MPI_Comm comm;
int * ndims;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(83);
  #endif

  returnVal = PMPI_Cartdim_get( comm, ndims );

  #ifdef PERF_TRACE
  TRACE_STOP(83);
  #endif

  return returnVal;
}

int  MPI_Dims_create( nnodes, ndims, dims )
int nnodes;
int ndims;
int * dims;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(84);
  #endif

  returnVal = PMPI_Dims_create( nnodes, ndims, dims );

  #ifdef PERF_TRACE
  TRACE_STOP(84);
  #endif

  return returnVal;
}

int   MPI_Graph_create( comm_old, nnodes, index, edges, reorder, comm_graph )
MPI_Comm comm_old;
int nnodes;
int * index;
int * edges;
int reorder;
MPI_Comm * comm_graph;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(85);
  #endif

  returnVal = PMPI_Graph_create( comm_old, nnodes, index, edges, reorder, comm_graph );

  #ifdef PERF_TRACE
  TRACE_STOP(85);
  #endif

  return returnVal;
}

int   MPI_Graph_get( comm, maxindex, maxedges, index, edges )
MPI_Comm comm;
int maxindex;
int maxedges;
int * index;
int * edges;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(86);
  #endif

  returnVal = PMPI_Graph_get( comm, maxindex, maxedges, index, edges );

  #ifdef PERF_TRACE
  TRACE_STOP(86);
  #endif

  return returnVal;
}

int   MPI_Graph_map( comm_old, nnodes, index, edges, newrank )
MPI_Comm comm_old;
int nnodes;
int * index;
int * edges;
int * newrank;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(87);
  #endif

  returnVal = PMPI_Graph_map( comm_old, nnodes, index, edges, newrank );

  #ifdef PERF_TRACE
  TRACE_STOP(87);
  #endif

  return returnVal;
}

int   MPI_Graph_neighbors( comm, rank, maxneighbors, neighbors )
MPI_Comm comm;
int rank;
int  maxneighbors;
int * neighbors;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(88);
  #endif

  returnVal = PMPI_Graph_neighbors( comm, rank, maxneighbors, neighbors );

  #ifdef PERF_TRACE
  TRACE_STOP(88);
  #endif

  return returnVal;
}

int   MPI_Graph_neighbors_count( comm, rank, nneighbors )
MPI_Comm comm;
int rank;
int * nneighbors;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(89);
  #endif

  returnVal = PMPI_Graph_neighbors_count( comm, rank, nneighbors );

  #ifdef PERF_TRACE
  TRACE_STOP(89);
  #endif

  return returnVal;
}

int   MPI_Graphdims_get( comm, nnodes, nedges )
MPI_Comm comm;
int * nnodes;
int * nedges;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(90);
  #endif

  returnVal = PMPI_Graphdims_get( comm, nnodes, nedges );

  #ifdef PERF_TRACE
  TRACE_STOP(90);
  #endif

  return returnVal;
}

int   MPI_Topo_test( comm, top_type )
MPI_Comm comm;
int * top_type;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(91);
  #endif

  returnVal = PMPI_Topo_test( comm, top_type );

  #ifdef PERF_TRACE
  TRACE_STOP(91);
  #endif

  return returnVal;
}

/******************************************************************
*                                                                 *
*                      MPI Other Functions                        *
*                                                                 *
******************************************************************/

/* LAM MPI defines MPI_Abort as a macro! We check for this and if
   it is defined that way, we change the MPI_Abort wrapper */

#if (defined(MPI_Abort) && defined(_ULM_MPI_H_))
int _MPI_Abort( MPI_Comm comm, int errorcode, char * file, int line)
#else
int  MPI_Abort( comm, errorcode )
MPI_Comm comm;
int errorcode;
#endif /* MPI_Abort & LAM MPI [LAM MPI] */
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(92);
  #endif

  returnVal = PMPI_Abort( comm, errorcode );

  #ifdef PERF_TRACE
  TRACE_STOP(92);
  #endif

  return returnVal;
}

int  MPI_Error_class( errorcode, errorclass )
int errorcode;
int * errorclass;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(93);
  #endif

  returnVal = PMPI_Error_class( errorcode, errorclass );

  #ifdef PERF_TRACE
  TRACE_STOP(93);
  #endif

  return returnVal;
}

int  MPI_Errhandler_create( function, errhandler )
MPI_Handler_function * function;
MPI_Errhandler * errhandler;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(94);
  #endif

  returnVal = PMPI_Errhandler_create( function, errhandler );

  #ifdef PERF_TRACE
  TRACE_STOP(94);
  #endif

  return returnVal;
}

int  MPI_Errhandler_free( errhandler )
MPI_Errhandler * errhandler;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(95);
  #endif

  returnVal = PMPI_Errhandler_free( errhandler );

  #ifdef PERF_TRACE
  TRACE_STOP(95);
  #endif

  return returnVal;
}

int  MPI_Errhandler_get( comm, errhandler )
MPI_Comm comm;
MPI_Errhandler * errhandler;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(96);
  #endif

  returnVal = PMPI_Errhandler_get( comm, errhandler );

  #ifdef PERF_TRACE
  TRACE_STOP(96);
  #endif

  return returnVal;
}

int  MPI_Error_string( errorcode, string, resultlen )
int errorcode;
char * string;
int * resultlen;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(97);
  #endif

  returnVal = PMPI_Error_string( errorcode, string, resultlen );

  #ifdef PERF_TRACE
  TRACE_STOP(97);
  #endif

  return returnVal;
}

int  MPI_Errhandler_set( comm, errhandler )
MPI_Comm comm;
MPI_Errhandler errhandler;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(98);
  #endif

  returnVal = PMPI_Errhandler_set( comm, errhandler );

  #ifdef PERF_TRACE
  TRACE_STOP(98);
  #endif

  return returnVal;
}


int  MPI_Get_processor_name( name, resultlen )
char * name;
int * resultlen;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(99);
  #endif

  returnVal = PMPI_Get_processor_name( name, resultlen );

  #ifdef PERF_TRACE
  TRACE_STOP(99);
  #endif

  return returnVal;
}


double  MPI_Wtick(  )
{
  double  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(100);
  #endif

  returnVal = PMPI_Wtick(  );

  #ifdef PERF_TRACE
  TRACE_STOP(100);
  #endif

  return returnVal;
}

double  MPI_Wtime(  )
{
  double  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(101);
  #endif

  returnVal = PMPI_Wtime(  );

  #ifdef PERF_TRACE
  TRACE_STOP(101);
  #endif

  return returnVal;
}

int  MPI_Address( location, address )
void * location;
MPI_Aint * address;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(102);
  #endif

  returnVal = PMPI_Address( location, address );

  #ifdef PERF_TRACE
  TRACE_STOP(102);
  #endif

  return returnVal;
}

int  MPI_Op_create( function, commute, op )
MPI_User_function * function;
int commute;
MPI_Op * op;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(103);
  #endif

  returnVal = PMPI_Op_create( function, commute, op );

  #ifdef PERF_TRACE
  TRACE_STOP(103);
  #endif

  return returnVal;
}

int  MPI_Op_free( op )
MPI_Op * op;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(104);
  #endif

  returnVal = PMPI_Op_free( op );

  #ifdef PERF_TRACE
  TRACE_STOP(104);
  #endif

  return returnVal;
}

int   MPI_Attr_delete( comm, keyval )
MPI_Comm comm;
int keyval;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(105);
  #endif

  returnVal = PMPI_Attr_delete( comm, keyval );

  #ifdef PERF_TRACE
  TRACE_STOP(105);
  #endif

  return returnVal;
}

int   MPI_Attr_get( comm, keyval, attr_value, flag )
MPI_Comm comm;
int keyval;
void * attr_value;
int * flag;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(106);
  #endif

  returnVal = PMPI_Attr_get( comm, keyval, attr_value, flag );

  #ifdef PERF_TRACE
  TRACE_STOP(106);
  #endif

  return returnVal;
}

int   MPI_Attr_put( comm, keyval, attr_value )
MPI_Comm comm;
int keyval;
void * attr_value;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(107);
  #endif

  returnVal = PMPI_Attr_put( comm, keyval, attr_value );

  #ifdef PERF_TRACE
  TRACE_STOP(107);
  #endif

  return returnVal;
}

int  MPI_Buffer_attach( buffer, size )
void * buffer;
int size;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(108);
  #endif

  returnVal = PMPI_Buffer_attach( buffer, size );

  #ifdef PERF_TRACE
  TRACE_STOP(108);
  #endif

  return returnVal;
}

int  MPI_Buffer_detach( buffer, size )
void * buffer;
int * size;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(109);
  #endif

  returnVal = PMPI_Buffer_detach( buffer, size );

  #ifdef PERF_TRACE
  TRACE_STOP(109);
  #endif

  return returnVal;
}

int   MPI_Get_elements( status, datatype, elements )
MPI_Status * status;
MPI_Datatype datatype;
int * elements;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(110);
  #endif

  returnVal = PMPI_Get_elements( status, datatype, elements );

  #ifdef PERF_TRACE
  TRACE_STOP(110);
  #endif

  return returnVal;
}

int  MPI_Get_count( status, datatype, count )
MPI_Status * status;
MPI_Datatype datatype;
int * count;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(111);
  #endif

  returnVal = PMPI_Get_count( status, datatype, count );

  #ifdef PERF_TRACE
  TRACE_STOP(111);
  #endif

  return returnVal;
}

int   MPI_Type_commit( datatype )
MPI_Datatype * datatype;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(112);
  #endif

  returnVal = PMPI_Type_commit( datatype );

  #ifdef PERF_TRACE
  TRACE_STOP(112);
  #endif

  return returnVal;
}

int  MPI_Type_contiguous( count, old_type, newtype )
int count;
MPI_Datatype old_type;
MPI_Datatype * newtype;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(113);
  #endif

  returnVal = PMPI_Type_contiguous( count, old_type, newtype );

  #ifdef PERF_TRACE
  TRACE_STOP(113);
  #endif

  return returnVal;
}

int  MPI_Type_extent( datatype, extent )
MPI_Datatype datatype;
MPI_Aint * extent;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(114);
  #endif

  returnVal = PMPI_Type_extent( datatype, extent );

  #ifdef PERF_TRACE
  TRACE_STOP(114);
  #endif

  return returnVal;
}

int   MPI_Type_free( datatype )
MPI_Datatype * datatype;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(115);
  #endif

  returnVal = PMPI_Type_free( datatype );

  #ifdef PERF_TRACE
  TRACE_STOP(115);
  #endif

  return returnVal;
}

int  MPI_Type_hindexed( count, blocklens, indices, old_type, newtype )
int count;
int * blocklens;
MPI_Aint * indices;
MPI_Datatype old_type;
MPI_Datatype * newtype;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(116);
  #endif

  returnVal = PMPI_Type_hindexed( count, blocklens, indices, old_type, newtype );

  #ifdef PERF_TRACE
  TRACE_STOP(116);
  #endif

  return returnVal;
}

int  MPI_Type_hvector( count, blocklen, stride, old_type, newtype )
int count;
int blocklen;
MPI_Aint stride;
MPI_Datatype old_type;
MPI_Datatype * newtype;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(117);
  #endif

  returnVal = PMPI_Type_hvector( count, blocklen, stride, old_type, newtype );

  #ifdef PERF_TRACE
  TRACE_STOP(117);
  #endif

  return returnVal;
}

int  MPI_Type_indexed( count, blocklens, indices, old_type, newtype )
int count;
int * blocklens;
int * indices;
MPI_Datatype old_type;
MPI_Datatype * newtype;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(118);
  #endif

  returnVal = PMPI_Type_indexed( count, blocklens, indices, old_type, newtype );

  #ifdef PERF_TRACE
  TRACE_STOP(118);
  #endif

  return returnVal;
}

int   MPI_Type_lb( datatype, displacement )
MPI_Datatype datatype;
MPI_Aint * displacement;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(119);
  #endif

  returnVal = PMPI_Type_lb( datatype, displacement );

  #ifdef PERF_TRACE
  TRACE_STOP(119);
  #endif

  return returnVal;
}

int   MPI_Type_size( datatype, size )
MPI_Datatype datatype;
int * size;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(120);
  #endif

  returnVal = PMPI_Type_size( datatype, size );

  #ifdef PERF_TRACE
  TRACE_STOP(120);
  #endif

  return returnVal;
}

int  MPI_Type_struct( count, blocklens, indices, old_types, newtype )
int count;
int * blocklens;
MPI_Aint * indices;
MPI_Datatype * old_types;
MPI_Datatype * newtype;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(121);
  #endif

  returnVal = PMPI_Type_struct( count, blocklens, indices, old_types, newtype );

  #ifdef PERF_TRACE
  TRACE_STOP(121);
  #endif
  return returnVal;
}

int   MPI_Type_ub( datatype, displacement )
MPI_Datatype datatype;
MPI_Aint * displacement;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(122);
  #endif

  returnVal = PMPI_Type_ub( datatype, displacement );

  #ifdef PERF_TRACE
  TRACE_STOP(122);
  #endif

  return returnVal;
}

int  MPI_Type_vector( count, blocklen, stride, old_type, newtype )
int count;
int blocklen;
int stride;
MPI_Datatype old_type;
MPI_Datatype * newtype;
{
  int  returnVal;

  #ifdef PERF_TRACE
  TRACE_START(123);
  #endif

  returnVal = PMPI_Type_vector( count, blocklen, stride, old_type, newtype );

  #ifdef PERF_TRACE
  TRACE_STOP(123);
  #endif

  return returnVal;
}

int   MPI_Unpack( inbuf, insize, position, outbuf, outcount, type, comm )
void * inbuf;
int insize;
int * position;
void * outbuf;
int outcount;
MPI_Datatype type;
MPI_Comm comm;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(124);
  #endif

  returnVal = PMPI_Unpack( inbuf, insize, position, outbuf, outcount, type, comm );

  #ifdef PERF_TRACE
  TRACE_STOP(124);
  #endif

  return returnVal;
}

int   MPI_Pack( inbuf, incount, type, outbuf, outcount, position, comm )
void * inbuf;
int incount;
MPI_Datatype type;
void * outbuf;
int outcount;
int * position;
MPI_Comm comm;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(125);
  #endif

  returnVal = PMPI_Pack( inbuf, incount, type, outbuf, outcount, position, comm );

  #ifdef PERF_TRACE
  TRACE_STOP(125);
  #endif

  return returnVal;
}

int   MPI_Pack_size( incount, datatype, comm, size )
int incount;
MPI_Datatype datatype;
MPI_Comm comm;
int * size;
{
  int   returnVal;

  #ifdef PERF_TRACE
  TRACE_START(126);
  #endif

  returnVal = PMPI_Pack_size( incount, datatype, comm, size );

  #ifdef PERF_TRACE
  TRACE_STOP(126);
  #endif

  return returnVal;
}
