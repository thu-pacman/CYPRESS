/***************************************************
*       Main tracing interface for libmpit         *
*     Modified by Jidong Zhai on July 19, 2012.    *
*     Add compress by clw. 201210.
***************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <mpi.h>

#include "trace.h"
#include "timer.h"
#include "typedef.h"
#include "intracomp.h"

#define  MAXBUF 128*1024

static char trace_buf[MAXBUF];
char* buf_ptr = trace_buf;
char* buf_ptr_max = trace_buf + MAXBUF - 1;

static char trace_name[100];
static gzFile fp_trace = Z_NULL;
int proc_id;
double entry_time = 0.0;
p2p_msg* p2p_ip;
coll_msg* coll_ip;
wait_eve* wait_ip;
waitall_eve* waitall_ip;
trace_info* trace_ip;
comm_eve*  comm_ip;
_uint64 trace_id = 0;
_uint16 mpi_id;

//add by clw
static node_st *now_st;

typedef struct branch_mask_ branch_mask;
struct branch_mask_
{
  int id;
  branch_mask *pre;
};
static branch_mask *tail_bm;

static loop_trace *loop_trace_free(loop_trace *pt, int nt)
{
  // free (pt - n, pt], return (pt - n)
  repeat_trace *rt;
  loop_trace *pm;
  while (nt--)
  {
    if (pt->dist < 0)
    {
      rt = (repeat_trace *)pt;
      loop_trace_free((loop_trace *)(rt->trace), rt->dist);
      pt = (loop_trace *)(rt->pre);
      free(rt);
    }
    else
    {
      pm = pt;
      pt = pt->pre;
      free(pm);
    }
  }
  return pt;
}

static int loop_compare(loop_trace *pn1, loop_trace *pn2, int nt)
{
  repeat_trace *rt1, *rt2;
  while (pn1 && pn2 && nt)
  {
    if (pn1->dist != pn2->dist)
      return -1;
    if (pn1->dist < 0)
    {
      rt1 = (repeat_trace *)pn1;
      rt2 = (repeat_trace *)pn2;
      if (rt1->dist != rt2->dist || rt1->times != rt2->times)
        return -1;
      if (loop_compare((loop_trace *)(rt1->trace), (loop_trace *)(rt2->trace), rt1->dist) != 0)
        return -1;
      pn1 = (loop_trace *)(rt1->pre);
      pn2 = (loop_trace *)(rt2->pre);
    }
    else
    {
      if (pn1->times != pn2->times || pn1->retimes != pn2->retimes)
        return -1;
      pn1 = pn1->pre;
      pn2 = pn2->pre;
    }
    nt--;
  }
  if (nt)
    return -1;
  return 0;
}

static loop_trace *loop_compress(loop_trace *pt)
{
  if (pt == NULL)
    return NULL;
  int nt;
  loop_trace *pt2;
  repeat_trace *rt;
  nt = 1;
  pt2 = pt->dist < 0 ? (loop_trace *)(((repeat_trace *)pt)->pre) : pt->pre;
  while (pt2)
  {
    if (pt2->dist < 0)
    {
      rt = (repeat_trace *)pt2;
      // try to match (pt - nt, pt] with rt
      if (rt->dist == nt && loop_compare((loop_trace *)(rt->trace), pt, nt) == 0)
      {
        loop_trace_free(pt, nt);
        rt->times++;
        // second time compression
        return loop_compress(pt2);
      }
    }
    if (loop_compare(pt2, pt, nt) == 0)
    {
      rt = (repeat_trace *)malloc(sizeof(repeat_trace));
      rt->tag = -1;
      rt->dist = nt;
      rt->times = 2;
      rt->trace = pt;
      rt->pre = loop_trace_free(pt2, nt);
      // set head to NULL, may not be necessary
      loop_trace *pp = pt;
      int i;
      for (i = 1; i < nt; i++)
        pp = pp->dist < 0 ? (loop_trace *)(((repeat_trace *)pp)->pre) : pp->pre;
      if (pp->dist < 0)
        ((repeat_trace *)pp)->pre = NULL;
      else
        pp->pre = NULL;
      // second time compression
      return loop_compress((loop_trace *)rt);
    }
    pt2 = pt2->dist < 0 ? (loop_trace *)(((repeat_trace *)pt2)->pre) : pt2->pre;
    nt++;
  }
  return pt;
}

static branch_trace *branch_trace_free(branch_trace *pt, int nt)
{
  // free (pt - n, pt], return (pt - n)
  repeat_trace *rt;
  branch_trace *pm;
  while (nt--)
  {
    if (pt->dist < 0)
    {
      rt = (repeat_trace *)pt;
      branch_trace_free((branch_trace *)(rt->trace), rt->dist);
      pt = (branch_trace *)(rt->pre);
      free(rt);
    }
    else
    {
      pm = pt;
      pt = pt->pre;
      free(pm);
    }
  }
  return pt;
}

static int branch_compare(branch_trace *pn1, branch_trace *pn2, int nt)
{
  repeat_trace *rt1, *rt2;
  while (pn1 && pn2 && nt)
  {
    if (pn1->dist != pn2->dist)
      return -1;
    if (pn1->dist < 0)
    {
      rt1 = (repeat_trace *)pn1;
      rt2 = (repeat_trace *)pn2;
      if (rt1->dist != rt2->dist || rt1->times != rt2->times)
        return -1;
      if (branch_compare((branch_trace *)(rt1->trace), (branch_trace *)(rt2->trace), rt1->dist) != 0)
        return -1;
      pn1 = (branch_trace *)(rt1->pre);
      pn2 = (branch_trace *)(rt2->pre);
    }
    else
    {
      if (pn1->torf != pn2->torf || pn1->retimes != pn2->retimes)
        return -1;
      pn1 = pn1->pre;
      pn2 = pn2->pre;
    }
    nt--;
  }
  if (nt)
    return -1;
  return 0;
}

static branch_trace *branch_compress(branch_trace *pt)
{
  if (pt == NULL)
    return NULL;
  int nt;
  branch_trace *pt2;
  repeat_trace *rt;
  nt = 1;
  pt2 = pt->dist < 0 ? (branch_trace *)(((repeat_trace *)pt)->pre) : pt->pre;
  while (pt2)
  {
    if (pt2->dist < 0)
    {
      rt = (repeat_trace *)pt2;
      // try to match (pt - nt, pt] with rt
      if (rt->dist == nt && branch_compare((branch_trace *)(rt->trace), pt, nt) == 0)
      {
        branch_trace_free(pt, nt);
        rt->times++;
        // second time compression
        return branch_compress(pt2);
      }
    }
    if (branch_compare(pt2, pt, nt) == 0)
    {
      rt = (repeat_trace *)malloc(sizeof(repeat_trace));
      rt->tag = -1;
      rt->dist = nt;
      rt->times = 2;
      rt->trace = pt;
      rt->pre = branch_trace_free(pt2, nt);
      // set head to NULL, may not be necessary
      branch_trace *pp = pt;
      int i;
      for (i = 1; i < nt; i++)
        pp = pp->dist < 0 ? (branch_trace *)(((repeat_trace *)pp)->pre) : pp->pre;
      if (pp->dist < 0)
        ((repeat_trace *)pp)->pre = NULL;
      else
        pp->pre = NULL;
      // second time compression
      return branch_compress((branch_trace *)rt);
    }
    pt2 = pt2->dist < 0 ? (branch_trace *)(((repeat_trace *)pt2)->pre) : pt2->pre;
    nt++;
  }
  return pt;
}

static bytes_node *bytes_free(bytes_node *pt, int nt)
{
  // free (pt - n, pt], return (pt - n)
  // n == -1 : free all
  repeat_trace *rt;
  bytes_node *pm;
  while (pt)
  {
    if (nt == 0)
      break;
    nt--;
    if (pt->dist < 0)
    {
      rt = (repeat_trace *)pt;
      bytes_free((bytes_node *)(rt->trace), rt->dist);
      pt = (bytes_node *)(rt->pre);
      free(rt);
    }
    else
    {
      pm = pt;
      pt = pt->pre;
      free(pm);
    }
  }
  return pt;
}

static int bytes_compare(bytes_node *pn1, bytes_node *pn2, int nt)
{
  // nt == -1 : compare all
  repeat_trace *rt1, *rt2;
  while (pn1 && pn2)
  {
    if (nt == 0)
      break;
    nt--;
    if (pn1->dist != pn2->dist)
      return -1;
    if (pn1->dist < 0)
    {
      rt1 = (repeat_trace *)pn1;
      rt2 = (repeat_trace *)pn2;
      if (rt1->dist != rt2->dist || rt1->times != rt2->times)
        return -1;
      if (bytes_compare((bytes_node *)(rt1->trace), (bytes_node *)(rt2->trace), rt1->dist) != 0)
        return -1;
      pn1 = (bytes_node *)(rt1->pre);
      pn2 = (bytes_node *)(rt2->pre);
    }
    else
    {
      if (pn1->size != pn2->size)
        return -1;
      pn1 = pn1->pre;
      pn2 = pn2->pre;
    }
  }
  if (nt > 0)
    return -1;
  if (nt < 0 && (pn1 || pn2))
    return -1;
  return 0;
}

static bytes_node *bytes_compress(bytes_node *pt)
{
  if (pt == NULL)
    return NULL;
  int nt;
  bytes_node *pt2;
  repeat_trace *rt;
  nt = 1;
  pt2 = pt->dist < 0 ? (bytes_node *)(((repeat_trace *)pt)->pre) : pt->pre;
  while (pt2)
  {
    if (pt2->dist < 0)
    {
      rt = (repeat_trace *)pt2;
      // try to match (pt - nt, pt] with rt
      if (rt->dist == nt && bytes_compare((bytes_node *)(rt->trace), pt, nt) == 0)
      {
        bytes_free(pt, nt);
        rt->times++;
        // second time compression
        return bytes_compress(pt2);
      }
    }
    if (bytes_compare(pt2, pt, nt) == 0)
    {
      rt = (repeat_trace *)malloc(sizeof(repeat_trace));
      rt->tag = -1;
      rt->dist = nt;
      rt->times = 2;
      rt->trace = pt;
      rt->pre = bytes_free(pt2, nt);
      // set head to NULL, may not be necessary
      bytes_node *pp = pt;
      int i;
      for (i = 1; i < nt; i++)
        pp = pp->dist < 0 ? (bytes_node *)(((repeat_trace *)pp)->pre) : pp->pre;
      if (pp->dist < 0)
        ((repeat_trace *)pp)->pre = NULL;
      else
        pp->pre = NULL;
      // second time compression
      return bytes_compress((bytes_node *)rt);
    }
    pt2 = pt2->dist < 0 ? (bytes_node *)(((repeat_trace *)pt2)->pre) : pt2->pre;
    nt++;
  }
  return pt;
}

static p2p_trace *p2p_trace_free(p2p_trace *pt, int nt)
{
  // free (pt - n, pt], return (pt - n)
  repeat_trace *rt;
  p2p_trace *pm;
  while (nt--)
  {
    if (pt->dist < 0)
    {
      rt = (repeat_trace *)pt;
      p2p_trace_free((p2p_trace *)(rt->trace), rt->dist);
      pt = (p2p_trace *)(rt->pre);
      free(rt);
    }
    else
    {
      pm = pt;
      pt = pt->pre;
      bytes_free(pm->bytes, -1);
      free(pm);
    }
  }
  return pt;
}

static int p2p_compare(p2p_trace *pn1, p2p_trace *pn2, int nt)
{
  repeat_trace *rt1, *rt2;
  while (pn1 && pn2 && nt)
  {
    if (pn1->dist != pn2->dist)
      return -1;
    if (pn1->dist < 0)
    {
      rt1 = (repeat_trace *)pn1;
      rt2 = (repeat_trace *)pn2;
      if (rt1->dist != rt2->dist || rt1->times != rt2->times)
        return -1;
      if (p2p_compare((p2p_trace *)(rt1->trace), (p2p_trace *)(rt2->trace), rt1->dist) != 0)
        return -1;
      pn1 = (p2p_trace *)(rt1->pre);
      pn2 = (p2p_trace *)(rt2->pre);
    }
    else
    {
      if (pn1->comm_id != pn2->comm_id || pn1->src != pn2->src ||
        pn1->tag != pn2->tag || pn1->dest != pn2->dest ||
        pn1->times != pn2->times || bytes_compare(pn1->bytes, pn2->bytes, -1) != 0)
        return -1;
      pn1 = pn1->pre;
      pn2 = pn2->pre;
    }
    nt--;
  }
  if (nt)
    return -1;
  return 0;
}

static p2p_trace *p2p_compress(p2p_trace *pt)
{
  if (pt == NULL)
    return NULL;
  int nt;
  p2p_trace *pt2;
  repeat_trace *rt;
  nt = 1;

  // case: all parameters but bytes are same.
  pt2 = pt->dist < 0 ? (p2p_trace *)(((repeat_trace *)pt)->pre) : pt->pre;
  if (pt2 && pt2->dist >= 0 &&
    pt->comm_id == pt2->comm_id && pt->src == pt2->src &&
    pt->tag == pt2->tag && pt->dest == pt2->dest)
  {
    pt2->times += pt->times;
    pt->bytes->pre = pt2->bytes;
    pt2->bytes = bytes_compress(pt->bytes);
    free(pt);
    pt = pt2;
    pt2 = pt->pre;
  }

  while (pt2)
  {
    if (pt2->dist < 0)
    {
      rt = (repeat_trace *)pt2;
      // try to match (pt - nt, pt] with rt
      if (rt->dist == nt && p2p_compare((p2p_trace *)(rt->trace), pt, nt) == 0)
      {
        p2p_trace_free(pt, nt);
        rt->times++;
        // second time compression
        return p2p_compress(pt2);
      }
    }
    if (p2p_compare(pt2, pt, nt) == 0)
    {
      rt = (repeat_trace *)malloc(sizeof(repeat_trace));
      rt->tag = -1;
      rt->dist = nt;
      rt->times = 2;
      rt->trace = pt;
      rt->pre = p2p_trace_free(pt2, nt);
      // set head to NULL, may not be necessary
      p2p_trace *pp = pt;
      int i;
      for (i = 1; i < nt; i++)
        pp = pp->dist < 0 ? (p2p_trace *)(((repeat_trace *)pp)->pre) : pp->pre;
      if (pp->dist < 0)
        ((repeat_trace *)pp)->pre = NULL;
      else
        pp->pre = NULL;
      return p2p_compress((p2p_trace *)rt);
    }
    pt2 = pt2->dist < 0 ? (p2p_trace *)(((repeat_trace *)pt2)->pre) : pt2->pre;
    nt++;
  }
  return pt;
}

static coll_trace *coll_trace_free(coll_trace *pt, int nt)
{
  // free (pt - n, pt], return (pt - n)
  repeat_trace *rt;
  coll_trace *pm;
  while (nt--)
  {
    if (pt->dist < 0)
    {
      rt = (repeat_trace *)pt;
      coll_trace_free((coll_trace *)(rt->trace), rt->dist);
      pt = (coll_trace *)(rt->pre);
      free(rt);
    }
    else
    {
      pm = pt;
      pt = pt->pre;
      free(pm);
    }
  }
  return pt;
}

static int coll_compare(coll_trace *pn1, coll_trace *pn2, int nt)
{
  repeat_trace *rt1, *rt2;
  while (pn1 && pn2 && nt)
  {
    if (pn1->dist != pn2->dist)
      return -1;
    if (pn1->dist < 0)
    {
      rt1 = (repeat_trace *)pn1;
      rt2 = (repeat_trace *)pn2;
      if (rt1->dist != rt2->dist || rt1->times != rt2->times)
        return -1;
      if (coll_compare((coll_trace *)(rt1->trace), (coll_trace *)(rt2->trace), rt1->dist) != 0)
        return -1;
      pn1 = (coll_trace *)(rt1->pre);
      pn2 = (coll_trace *)(rt2->pre);
    }
    else
    {
      if (pn1->comm_id != pn2->comm_id || pn1->root_id != pn2->root_id ||
        pn1->send_bytes != pn2->send_bytes || pn1->recv_bytes != pn2->recv_bytes)
        return -1;
      pn1 = pn1->pre;
      pn2 = pn2->pre;
    }
    nt--;
  }
  if (nt)
    return -1;
  return 0;
}

static coll_trace *coll_compress(coll_trace *pt)
{
  if (pt == NULL)
    return NULL;
  int nt;
  coll_trace *pt2;
  repeat_trace *rt;
  nt = 1;
  pt2 = pt->dist < 0 ? (coll_trace *)(((repeat_trace *)pt)->pre) : pt->pre;
  while (pt2)
  {
    if (pt2->dist < 0)
    {
      rt = (repeat_trace *)pt2;
      // try to match (pt - nt, pt] with rt
      if (rt->dist == nt && coll_compare((coll_trace *)(rt->trace), pt, nt) == 0)
      {
        coll_trace_free(pt, nt);
        rt->times++;
        // second time compression
        return coll_compress(pt2);
      }
    }
    if (coll_compare(pt2, pt, nt) == 0)
    {
      rt = (repeat_trace *)malloc(sizeof(repeat_trace));
      rt->tag = -1;
      rt->dist = nt;
      rt->times = 2;
      rt->trace = pt;
      rt->pre = coll_trace_free(pt2, nt);
      // set head to NULL, may not be necessary
      coll_trace *pp = pt;
      int i;
      for (i = 1; i < nt; i++)
        pp = pp->dist < 0 ? (coll_trace *)(((repeat_trace *)pp)->pre) : pp->pre;
      if (pp->dist < 0)
        ((repeat_trace *)pp)->pre = NULL;
      else
        pp->pre = NULL;
      // second time compression
      return coll_compress((coll_trace *)rt);
    }
    pt2 = pt2->dist < 0 ? (coll_trace *)(((repeat_trace *)pt2)->pre) : pt2->pre;
    nt++;
  }
  return pt;
}


#ifdef MPICH2
    #define SHIFT(COMM_ID) (((COMM_ID&0xf0000000)>>24) + (COMM_ID&0x0000000f))
#else
    #define SHIFT(COMM_ID) COMM_ID
#endif

void TRACE_START(int _mpi_id){

    entry_time = current_time();
    mpi_id = _mpi_id;
}

void TRACE_STOP(int mpi_id){}

void TRACE_INIT(int process_id, int size){

    proc_id = process_id;
    trace_id = 0;
    memset(trace_buf, 0, MAXBUF);
    
    sprintf(trace_name, "mpi_trace.%d", proc_id);

    #ifdef DEBUG
    printf("p2p 52=%d, coll = 52=%d\n", sizeof(p2p_msg), sizeof(coll_msg)); 
    #endif

    if ( (fp_trace = gzopen(trace_name, "w")) == Z_NULL){
        printf("Error: Could not open MPI Trace file %s, Abort!\n", trace_name);
        exit(1);
    }

    trace_ip = (trace_info*)buf_ptr;
    trace_ip -> mpi_id = mpi_id;
    trace_ip -> comm_id = SHIFT(MPI_COMM_WORLD);
    trace_ip -> proc_size = size;
    trace_ip -> exit_time = entry_time;
    buf_ptr += sizeof(trace_info);
    trace_id++;
  now_st=compress_init(proc_id);
  tail_bm=NULL;
}

void TRACE_FLUSH(){
    gzwrite(fp_trace, (char*)trace_buf, MAXBUF);
    buf_ptr = trace_buf;
    memset(trace_buf, 0, MAXBUF);
}

void TRACE_FINISH(){

    if ((sizeof(trace_info)+buf_ptr-1) > buf_ptr_max)
        TRACE_FLUSH();

    // Hard coding
    mpi_id   = 2;

    trace_ip = (trace_info*)buf_ptr;
    trace_ip -> mpi_id = mpi_id;
    trace_ip -> exit_time = current_time();
    buf_ptr += sizeof(trace_info);
    
    TRACE_FLUSH();
    gzclose(fp_trace);
  compress_fini();
} 

void TRACE_P2P(int source, int dest, int tag, int commid, int size){

    if ((sizeof(p2p_msg)+buf_ptr-1) > buf_ptr_max)
        TRACE_FLUSH();
  double time;
    p2p_ip = (p2p_msg*)buf_ptr; 
    p2p_ip -> mpi_id = mpi_id;
    #ifdef DEBUG
    if((mpi_id!=8)&&(mpi_id!=10)&&(mpi_id!=12)&&(mpi_id!=15))
        printf("mpi_id:%d\n", mpi_id);
    #endif
    p2p_ip -> entry_time = entry_time;
    p2p_ip -> id = trace_id++;
    p2p_ip -> src = source;
    p2p_ip -> dest = dest;
    p2p_ip -> tag = tag;
    p2p_ip -> comm_id = SHIFT(commid);
    p2p_ip -> length = size;
  time=current_time();
    p2p_ip -> exit_time = time;
    buf_ptr += sizeof(p2p_msg);

  //add by clw
  now_st=now_st->next;
  if((now_st->tag!=5)||(now_st->id!=mpi_id))
  {
    printf("error: p2p tag/id not match.\n");
    return;
  }
  p2p_trace *pt;
  pt=(p2p_trace*)malloc(sizeof(p2p_trace));
  pt->dist=0;
  pt->comm_id=SHIFT(commid);
  pt->src=source;
  pt->tag=tag;
#ifdef MPIZIP_NO_TAG
  pt->tag=0;
#endif
  pt->dest=dest;
  pt->times = 1;
  pt->bytes = (bytes_node *)malloc(sizeof(bytes_node));
  pt->bytes->dist = 0;
  pt->bytes->size = size;
  pt->bytes->pre = NULL;
  pt->pre=(p2p_trace*)now_st->trace;
  now_st->times++;
  time=time-entry_time;
  now_st->sum_time+=time;
  now_st->sum_sqr_time+=time*time;
  now_st->trace=(void*)p2p_compress(pt);
}

void TRACE_WAIT(_uint64 id){

    if ((sizeof(wait_eve)+buf_ptr-1) > buf_ptr_max)
        TRACE_FLUSH();
  double time;
    wait_ip = (wait_eve*)buf_ptr;
    wait_ip -> mpi_id = mpi_id;
    #ifdef DEBUG
    if((mpi_id!=25))
    printf("wait mpi_id:%d\n", mpi_id);
    #endif
    wait_ip -> p2p_id = id;
    wait_ip -> id = trace_id++;
    wait_ip -> entry_time = entry_time;
  time=current_time();
    wait_ip -> exit_time  = time;
    buf_ptr += sizeof(wait_eve);

  //add by clw
  now_st=now_st->next;
  if((now_st->tag!=7)||(now_st->id!=mpi_id))
  {
    printf("error: wait tag/id not match.\n");
    return;
  }
  //TODO
  now_st->times++;
  time=time-entry_time;
  now_st->sum_time+=time;
  now_st->sum_sqr_time+=time*time;
  //TODO
}

void TRACE_WAITALL(_uint32 count, _uint64* buf){

    int i;
    _uint64* _uint64_ptr;

    if (count*sizeof(_uint64)+sizeof(waitall_eve)> MAXBUF){
        printf("buffer size is limited, cannot load all of waiting p2p events\n");
        exit(1);
    }else if( (buf_ptr + sizeof(waitall_eve) + count*sizeof(_uint64) - 1) > buf_ptr_max )
        TRACE_FLUSH();
  double time;
    waitall_ip = (waitall_eve*) buf_ptr;
    waitall_ip -> mpi_id = mpi_id;
    waitall_ip -> count = count;
    waitall_ip -> id = trace_id++;
    waitall_ip -> entry_time = entry_time;
  time=current_time();
    waitall_ip -> exit_time  = time;
    buf_ptr += sizeof(waitall_eve);
    _uint64_ptr = (_uint64*) buf_ptr;
    for(i=0; i<count; i++){
        _uint64_ptr[i] = buf[i];
    }
    buf_ptr += count*sizeof(_uint64);

  //add by clw
  now_st=now_st->next;
  if((now_st->tag!=8)||(now_st->id!=mpi_id))
  {
    printf("error: waitall tag/id not match.\n");
    return;
  }
  //TODO
  now_st->times++;
  time=time-entry_time;
  now_st->sum_time+=time;
  now_st->sum_sqr_time+=time*time;
  //TODO
}

void TRACE_COMM(int commid, int count, int*buf){

    int i;
    _uint32* _uint32_ptr;

    if (count*sizeof(_uint32)+sizeof(comm_eve)> MAXBUF){
        printf("buffer size is limited, cannot load all of  comm_id events\n");
        exit(1);
    }else if( (buf_ptr + sizeof(comm_eve) + count*sizeof(_uint32) - 1) > buf_ptr_max )
        TRACE_FLUSH();
    
    comm_ip = (comm_eve*) buf_ptr;
    comm_ip -> mpi_id = mpi_id;
    comm_ip -> comm_id = SHIFT(commid);
    comm_ip -> count = count;
    comm_ip -> id = trace_id++;
    buf_ptr += sizeof(comm_eve);
    _uint32_ptr = (_uint32*) buf_ptr;
    for(i=0; i<count; i++)
        _uint32_ptr[i] = buf[i];
    buf_ptr += count*sizeof(_uint32);
  //TODO clw
  now_st=now_st->next;
  if((now_st->tag!=9)||(now_st->id!=mpi_id))
  {
    printf("error: comm tag/id not match.\n");
    return;
  }
  now_st->times++;
  //TODO
}

void TRACE_COLL(int sendbytes, int recvbytes, int commid, int root){

    if ((sizeof(coll_msg)+buf_ptr-1) > buf_ptr_max)
        TRACE_FLUSH();
  double time;
    coll_ip = (coll_msg*)buf_ptr;   
    coll_ip -> mpi_id = mpi_id;
    coll_ip -> entry_time = entry_time;
    coll_ip -> id = trace_id++;
    coll_ip -> comm_id = SHIFT(commid);
    coll_ip -> root_id = root;
    coll_ip -> send_bytes = sendbytes;
    coll_ip -> recv_bytes = recvbytes;
  time=current_time();
    coll_ip -> exit_time = time;
    buf_ptr += sizeof(coll_msg);

  //add by clw
  now_st=now_st->next;
  if((now_st->tag!=6)||(now_st->id!=mpi_id))
  {
    printf("error: coll tag/id not match.\n");
    return;
  }
  coll_trace *pt;
  pt=(coll_trace*)malloc(sizeof(coll_trace));
  pt->dist=0;
  pt->comm_id=SHIFT(commid);
  pt->root_id=root;
  pt->send_bytes=sendbytes;
  pt->recv_bytes=recvbytes;
  pt->pre=(coll_trace*)now_st->trace;
  now_st->times++;
  time=time-entry_time;
  now_st->sum_time+=time;
  now_st->sum_sqr_time+=time*time;
  now_st->trace=(void*)coll_compress(pt);
}


//add by clw
extern int mpi_pattern(const int pattern,const int id)
{
//lazy to validate tag & check null pointer.
  node_st *tst;
  tst=now_st->next;
  if(tst==NULL)
  {
//    printf("warning: null struct in pattern.\n");
    return -1;
  }
  if(pattern==1)
  {
    if(tst->id!=id)
    {
 //     printf("warning: loop id not match.\n");
      return -1;
    }
    if((tst->tag!=0)&&(tst->tag!=1))
    {
  //    printf("waring: loop tag not match.\n");
      return -1;
    }
    now_st=tst;
    loop_trace *lt;
    if(now_st->tag==0)
    {
      lt=(loop_trace*)malloc(sizeof(loop_trace));
      lt->dist=0;
      lt->times=0;
      lt->retimes=1;
      lt->pre=(loop_trace*)now_st->trace;
      now_st->trace=(void*)lt;
      now_st->tag=1;
    }
    else if(now_st->tag==1)
    {
      lt=(loop_trace*)now_st->trace;
      lt->times++;
      now_st->times++;
    }
  }
  else if(pattern==2)
  {
    if(tst->tag!=2)
    {
//      printf("warning: branch tag not match.\n");
      return -1;
    }
    if((tst->id!=id)&&((tst->other==NULL)||(tst->other->id!=id)))
    {
 //     printf("warning: branch id not match.\n");
      return -1;
    }
    branch_trace *bt;
    now_st=tst;
    now_st->times++;
    bt=(branch_trace*)now_st->trace;
    branch_mask *tbm;
    //TODO: there maybe multiple branch_mask have the same id.
    tbm=(branch_mask *)malloc(sizeof(branch_mask));
    tbm->id=now_st->id;
    tbm->pre=tail_bm;
    tail_bm=tbm;
    if(now_st->id==id)
    {
      if((bt!=NULL)&&(bt->dist==0)&&(bt->torf==1))
      {
        bt->retimes++;
        now_st->trace=branch_compress(bt);
      }
      else{
        bt=(branch_trace*)malloc(sizeof(branch_trace));
        bt->dist=0;
        bt->torf=1;
        bt->retimes=1;
        bt->pre=(branch_trace*)now_st->trace;
        now_st->trace=branch_compress(bt);
      }
    }
    else{
      if((bt!=NULL)&&(bt->dist==0)&&(bt->torf==0))
      {
        bt->retimes++;
        now_st->trace=branch_compress(bt);
      }
      else{
        bt=(branch_trace*)malloc(sizeof(branch_trace));
        bt->dist=0;
        bt->torf=0;
        bt->retimes=1;
        bt->pre=(branch_trace*)now_st->trace;
        now_st->trace=branch_compress(bt);
      }
      now_st=now_st->other;
    }
  }
  return 0;
}

extern int mpi_pattern_exit(const int pattern,const int id)
{
  if(pattern==1)
  {
    if(now_st->id!=id)
    {
      if((now_st->next==NULL)||(now_st->next->id!=id))
      {
        //printf("warning: loop exit id not match.\n");
        return -1;
      }
      mpi_pattern(1,id);
    }
    if(now_st->tag!=1)
    {
//      printf("warning: loop exit tag not match.\n");
      return -1;
    }
    loop_trace *lt;
    lt=(loop_trace*)now_st->trace;
    if((lt->pre!=NULL)&&(lt->pre->dist==0)&&(lt->times==lt->pre->times))
    {
      lt->pre->retimes++;
      now_st->trace=loop_compress(lt->pre);
      free(lt);
    }
    now_st->tag=0;
    now_st=now_st->other;
  }
  else if(pattern==2)
  {
    branch_mask *tbm;
    tbm=tail_bm;
    while(tbm!=NULL)
    {
      if(tbm->id==id)
      {
        while(tail_bm->id!=id)
        {
          tbm=tail_bm->pre;
          free(tail_bm);
          tail_bm=tbm;
        }
        tbm=tail_bm->pre;
        free(tail_bm);
        tail_bm=tbm;
        return -1;
      }
      tbm=tbm->pre;
    }
    node_st *tst;
    branch_trace *bt;
    tst=now_st->next;
    if((tst!=NULL)&&(tst->tag==2)&&(tst->id==id))
    {
      now_st=now_st->next;
      now_st->times++;
      bt=(branch_trace*)now_st->trace;
      if((bt!=NULL)&&(bt->dist==0)&&(bt->torf==0))
      {
        bt->retimes++;
        now_st->trace=branch_compress(bt);
      }
      else{
        bt=(branch_trace*)malloc(sizeof(branch_trace));
        bt->dist=0;
        bt->torf=0;
        bt->retimes=1;
        bt->pre=(branch_trace*)now_st->trace;
        now_st->trace=branch_compress(bt);
      }
      now_st=now_st->other;
    }
  }
  return 0;
}
