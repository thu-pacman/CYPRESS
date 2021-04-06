//clw.201211.
#ifndef INTERCOMP_H
#define INTERCOMP_H

#include <stdint.h>
#include "intracomp.h"

typedef struct node_info_ node_info;
struct node_info_
{
  uint32_t times;
  double sum_time;
  double sum_sqr_time;
};

typedef struct statis_info_ statis_info;
struct statis_info_
{
  uint32_t max_times;
  uint32_t min_times;
  uint32_t total_times;
  double max_time;
  double min_time;
  double total_time;
  double total_sqr_time;
};

typedef struct proc_node_ proc_node;
struct proc_node_
{
  uint32_t pid;
  uint8_t bol;
};

typedef struct proc_stack_ proc_stack;
struct proc_stack_
{
  uint32_t begin_id;
  uint32_t end_id;
  proc_stack *next;
};

typedef struct compressed_ compressed;
struct compressed_
{
  uint32_t key;           //hash key
  uint16_t tag;            //same as tag in convert_st, just for convenient.
  int bol;                //use in binary write, unuse 0, used 1.
  uint32_t nproc;
  proc_stack *procs;
  void *converted_trace;
  compressed *next;
};

typedef struct convert_st_ convert_st;
struct convert_st_
{
  uint16_t tag;               //0: f(x)=x, 1: f(x)=x-n
  compressed *ctrace;
  convert_st *next;
};

typedef struct node_st2_ node_st2;
struct node_st2_
{
//1=loop, 2=branch, 5=p2p, 6=coll, 7=wait, 8=waitall, 9=comm, 13=root
  uint16_t tag;
  uint16_t id;
  uint32_t nchild;
  statis_info *sinfo;
  node_info *info;
  void *trace;
  convert_st *fcs;
  node_st2 *next;
  node_st2 *other;
};

// the bytes wrapper for each proc
typedef struct bytes_trace_ bytes_trace;
struct bytes_trace_
{
  uint32_t pid;
  void *bytes;
  bytes_trace *next;
};

typedef struct src_node_ src_node;
struct src_node_
{
  int16_t dist;
  uint32_t srcpid;
  src_node *pre;
};

// the src wrapper for each proc
typedef struct src_trace_ src_trace;
struct src_trace_
{
  uint32_t pid;
  void *src;
  src_trace *next;
};

// special p2p trace with src_list
typedef struct p2p_trace_src_ p2p_trace_src;
struct p2p_trace_src_
{
  int16_t dist;             //also tag = 1
  uint16_t comm_id;
  void *src;
  int16_t tag;
  int16_t dest;
  uint32_t times;
  void *bytes;
  p2p_trace_src *pre;
};

#endif
