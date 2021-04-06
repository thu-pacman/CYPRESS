//clw.201210.
#ifndef INTRACOMP_H
#define INTRACOMP_H

#include <stdint.h>

typedef struct node_st_ node_st;
struct node_st_
{
//0=new loop, 1=now loop, 2=branch, 5=p2p, 6=coll, 7=wait, 8=waitall, 9=comm, 13=root
  uint16_t tag;
  uint16_t id;
  uint32_t times;
  union
  {
    double sum_time;
    uint32_t nchild;    //use by root node, loop node, if node, else node
  };
  double sum_sqr_time;
  void *trace;
  node_st *next;
  node_st *other;
};

typedef struct else_stack_ else_stack;
struct else_stack_
{
  int tag;
  node_st *elst;
  else_stack *pre;
};

typedef struct loop_trace_ loop_trace;
struct loop_trace_
{
  int16_t dist;             //also tag
  uint32_t times;
  uint32_t retimes;
  loop_trace *pre;
};

typedef struct branch_trace_ branch_trace;
struct branch_trace_
{
  int16_t dist;             //also tag
  uint16_t torf;            //0=false, 1=true
  uint32_t retimes;
  branch_trace *pre;
};

typedef struct repeat_trace_ repeat_trace;
struct repeat_trace_
{
  int16_t tag;              //tag=-1
  int16_t dist;
  uint32_t times;
  void *trace;              //repeat pattern
  void *pre;
};

typedef struct bytes_node_ bytes_node;
struct bytes_node_
{
  int16_t dist;
  uint32_t size;
  bytes_node *pre;
};

typedef struct p2p_trace_ p2p_trace;
struct p2p_trace_
{
  int16_t dist;             //also tag = 0
  uint16_t comm_id;
  int16_t src;
  int16_t tag;
  int16_t dest;
  uint32_t times;
  #ifdef INTER_COMP
  void *bytes;
  #else
  bytes_node *bytes;
  #endif
  p2p_trace *pre;
};

typedef struct coll_trace_ coll_trace;
struct coll_trace_
{
  int16_t dist;
  uint16_t comm_id;
  uint16_t root_id;
  uint32_t send_bytes;
  uint32_t recv_bytes;
  coll_trace *pre;
};

extern node_st *compress_init(const int pid);
extern int compress_fini();

#endif

