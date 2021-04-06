//clw.201211.
//yfk.201501.
#define INTER_COMP

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "intercomp.h"
#include "intracomp.h"
#include "timer.h"

static uint32_t np;
static uint32_t *proc_list;
FILE *fp;
// multithread variables
node_st2 **root_queue;
int head, tail;
int max_queue_size;
int building_thread_count;
int merging_thread_count;
int max_building_thread_count;
int max_merging_thread_count;
char **argvs;
pthread_mutex_t qmutex;

static convert_st *new_convert_st(uint16_t tag)
{
  convert_st *cs,*ts;
  cs=(convert_st*)malloc(sizeof(convert_st));
  cs->tag=0;
  cs->ctrace=NULL;
  cs->next=NULL;
  if(tag==5)
  {
    ts=(convert_st*)malloc(sizeof(convert_st));
    cs->next=ts;
    ts->tag=1;
    ts->ctrace=NULL;
    ts->next=(convert_st*)malloc(sizeof(convert_st));
    ts=ts->next;
    ts->tag=2;
    ts->ctrace=NULL;
    ts->next=(convert_st*)malloc(sizeof(convert_st));
    ts=ts->next;
    ts->tag=3;
    ts->ctrace=NULL;
    ts->next=NULL;

  }
  return cs;
}

static node_st2 *new_node_st2(uint16_t tag,uint16_t id,uint32_t nchild)
{
  node_st2 *nst;
  statis_info *si;
  nst=(node_st2*)malloc(sizeof(node_st2));
  nst->tag=tag;
  nst->id=id;
  nst->nchild=nchild;
  si=(statis_info*)malloc(sizeof(statis_info));
  si->max_times=0;
  si->min_times=-1;
  si->total_times=0;
  si->max_time=0;
  si->min_time=-1;
  si->total_time=0;
  si->total_sqr_time=0;
  nst->sinfo=si;
  nst->info=(node_info*)malloc(sizeof(node_info));
  nst->trace=(void*)malloc(sizeof(void*));
  nst->fcs=new_convert_st(tag);
  nst->next=NULL;
  nst->other=NULL;
  return nst;
}

// hash p2p trace without bytes, src infomation
static uint32_t trace_hash_func0(void *trace, uint16_t tag, uint32_t pid)
{
  uint32_t hash = 0;
  uint32_t seed = 131;
  repeat_trace *rt;
  if (tag == 1)
  {
    loop_trace *t = (loop_trace *)trace;
    while (t)
    {
      hash = hash * seed + t->dist;
      if (t->dist == -1)
      {
        rt = (repeat_trace*)t;
        hash = hash * seed + rt->dist;
        hash = hash * seed + rt->times;
        t = (loop_trace *)rt->pre;
      }
      else
      {
        hash = hash * seed + t->times;
        hash = hash * seed + t->retimes;
        t = t->pre;
      }
    }
  }
  else if (tag == 2)
  {
    branch_trace *t = (branch_trace *)trace;
    while (t)
    {
      hash = hash * seed + t->dist;
      if (t->dist == -1)
      {
        rt = (repeat_trace*)t;
        hash = hash * seed + rt->dist;
        hash = hash * seed + rt->times;
        t = (branch_trace *)rt->pre;
      }
      else
      {
        hash = hash * seed + t->torf;
        hash = hash * seed + t->retimes;
        t = t->pre;
      }
    }
  }
  else if(tag == 5)
  {
    p2p_trace *t;
    t = (p2p_trace *)trace;
    while (t)
    {
      hash = hash * seed + t->dist;
      if (t->dist == -1)
      {
        rt = (repeat_trace *)t;
        hash = hash * seed + rt->dist;
        hash = hash * seed + rt->times;
        t = (p2p_trace *)rt->pre;
      }
      else if (t->dist == 1)
      {
        p2p_trace_src *ps = (p2p_trace_src *)t;
        hash = hash * seed + ps->comm_id;
        hash = hash * seed + ps->tag;
        hash = hash * seed + ps->dest;
        hash = hash * seed + ps->times;
        t = (p2p_trace *)(ps->pre);
      }
      else
      {
        hash = hash * seed + t->comm_id;
        hash = hash * seed + t->tag;
        hash = hash * seed + t->dest;
        hash = hash * seed + t->times;
        t = t->pre;
      }
    }
  }
  else if(tag == 6)
  {
    coll_trace *t;
    t = (coll_trace *)trace;
    while (t)
    {
      hash = hash * seed + t->dist;
      if (t->dist == -1)
      {
        rt = (repeat_trace *)t;
        hash = hash * seed + rt->dist;
        hash = hash * seed + rt->times;
        t = (coll_trace *)rt->pre;
      }
      else
      {
        hash = hash * seed + t->comm_id;
        hash = hash * seed + t->root_id;
        hash = hash * seed + t->send_bytes;
        hash = hash * seed + t->recv_bytes;
        t = t->pre;
      }
    }
  }
  else if(tag==7)
  {
    //TODO
    return 0;
  }
  else if(tag==8)
  {
    //TODO
    return 0;
  }
  else if(tag==9)
  {
    //TODO
    return 0;
  }
  else printf("error: unknow tag.\n");
  return hash;
}

static uint32_t trace_hash_func1(void *trace, uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag1 but trace tag not 5.\n");
    return 0;
  }
  uint32_t hash = 0;
  uint32_t seed = 131;
  repeat_trace *rt;
  p2p_trace *t;
  t = (p2p_trace *)trace;
  while (t)
  {
    hash = hash * seed + t->dist;
    if (t->dist == -1)
    {
      rt = (repeat_trace *)t;
      hash = hash * seed + rt->dist;
      hash = hash * seed + rt->times;
      t = (p2p_trace *)rt->pre;
    }
    else if (t->dist == 1)
    {
      p2p_trace_src *ps = (p2p_trace_src *)t;
      hash = hash * seed + ps->comm_id;
      hash = hash * seed + ps->tag;
      hash = hash * seed + ps->dest - pid;
      hash = hash * seed + ps->times;
      t = (p2p_trace *)(ps->pre);
    }
    else
    {
      hash = hash * seed + t->comm_id;
      hash = hash * seed + t->tag;
      hash = hash * seed + t->dest - pid;
      hash = hash * seed + t->times;
      t = t->pre;
    }
  }
  return hash;
}

static uint32_t trace_hash_func2(void *trace, uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag1 but trace tag not 5.\n");
    return 0;
  }
  uint32_t hash = 0;
  uint32_t seed = 131;
  p2p_trace *t;
  t = (p2p_trace *)trace;
  repeat_trace *rt;
  while (t)
  {
    hash = hash * seed + t->dist;
    if (t->dist == -1)
    {
      rt = (repeat_trace *)t;
      hash = hash * seed + rt->dist;
      hash = hash * seed + rt->times;
      t = (p2p_trace *)rt->pre;
    }
    else if (t->dist == 1)
    {
      p2p_trace_src *ps = (p2p_trace_src *)t;
      hash = hash * seed + ps->comm_id;
      hash = hash * seed + ps->tag;
      hash = hash * seed + ps->dest - pid;
      hash = hash * seed + ps->times;
      t = (p2p_trace *)(ps->pre);
    }
    else
    {
      hash = hash * seed + t->comm_id;
      hash = hash * seed + t->tag;
      hash = hash * seed + t->dest - pid;
      hash = hash * seed + t->times;
      t = t->pre;
    }
  }
  return hash;
}

static uint32_t trace_hash_func3(void *trace, uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag1 but trace tag not 5.\n");
    return 0;
  }
  uint32_t hash = 0;
  uint32_t seed = 131;
  p2p_trace *t;
  repeat_trace *rt;
  t = (p2p_trace *)trace;
  while (t)
  {
    hash = hash * seed + t->dist;
    if (t->dist == -1)
    {
      rt = (repeat_trace *)t;
      hash = hash * seed + rt->dist;
      hash = hash * seed + rt->times;
      t = (p2p_trace *)rt->pre;
    }
    else if (t->dist == 1)
    {
      p2p_trace_src *ps = (p2p_trace_src *)t;
      hash = hash * seed + ps->comm_id;
      hash = hash * seed + ps->tag;
      hash = hash * seed + ps->dest;
      hash = hash * seed + ps->times;
      t = (p2p_trace *)(ps->pre);
    }
    else
    {
      hash = hash * seed + t->comm_id;
      hash = hash * seed + t->tag;
      hash = hash * seed + t->dest;
      hash = hash * seed + t->times;
      t = t->pre;
    }
  }
  return hash;
}

// work with hash key
static compressed *trace_insert(compressed *target, compressed *head)
{
  if (!head)
    return target;
  if (head->key >= target->key)
  {
    target->next = head;
    return target;
  }
  compressed *now = head;
  while (now->next && now->next->key < target->key)
    now = now->next;
  target->next = now->next;
  now->next = target;
  return head;
}

static int proc_stack_add(compressed *target, uint32_t pid)
{
  proc_stack *ps,*temp;
  ps=target->procs;
  if(ps->begin_id>pid)
  {
    target->nproc++;
    if(ps->begin_id==pid+1)ps->begin_id=pid;
    else{
      temp=(proc_stack*)malloc(sizeof(proc_stack));
      temp->begin_id=pid;
      temp->end_id=pid;
      temp->next=target->procs;
      target->procs=temp;
    }
  }
  if(ps->end_id>=pid)return 0;
  while((ps->next!=NULL)&&(ps->next->end_id<pid))ps=ps->next;
  if((ps->next!=NULL)&&(ps->next->begin_id<=pid))return 0;
  target->nproc++;
  if(ps->end_id+1==pid)
  {
    if((ps->next!=NULL)&&(ps->next->begin_id==pid+1))
    {
      ps->end_id=ps->next->end_id;
      ps->next=ps->next->next;
      free(ps->next);
    }
    else ps->end_id=pid;
  }
  else if((ps->next!=NULL)&&(ps->next->begin_id==pid+1))
  {
    ps->next->begin_id=pid;
  }
  else{
    temp=(proc_stack*)malloc(sizeof(proc_stack));
    temp->begin_id=pid;
    temp->end_id=pid;
    temp->next=ps->next;
    ps->next=temp;
  }
  return 0;
}

static bytes_node *bytes_copy(bytes_node *a)
{
  if (a == NULL) return NULL;
  if (a->dist < 0)
  {
    repeat_trace *ra = (repeat_trace *)a;
    repeat_trace *rb = (repeat_trace *)malloc(sizeof(repeat_trace));
    rb->tag = ra->tag;
    rb->dist = ra->dist;
    rb->times = ra->times;
    rb->trace = bytes_copy((bytes_node *)(ra->trace));
    rb->pre = bytes_copy((bytes_node *)(ra->pre));
    return (bytes_node *)rb;
  }
  else
  {
    bytes_node *b = (bytes_node *)malloc(sizeof(bytes_node));
    b->dist = a->dist;
    b->size = a->size;
    b->pre = bytes_copy(a->pre);
    return b;
  }
}

static bytes_trace *bytes_convert(bytes_trace *a)
{
  bytes_trace *b = (bytes_trace *)malloc(sizeof(bytes_trace));
  b->pid = a->pid;
  b->bytes = bytes_copy((bytes_node *)(a->bytes));
  b->next = NULL;
  return b;
}

static int bytes_compare_func(bytes_node *a, bytes_node *b)
{
  repeat_trace *ra,*rb;
  while((a!=NULL)&&(b!=NULL))
  {
    if(a->dist!=b->dist)return 0;
    if(a->dist==-1)
    {
      ra=(repeat_trace*)a;
      rb=(repeat_trace*)b;
      if((ra->dist!=rb->dist)||(ra->times!=rb->times))return 0;
      if (bytes_compare_func(ra->trace, rb->trace) != 1)
        return 0;
      a=(bytes_node*)ra->pre;
      b=(bytes_node*)rb->pre;
    }
    else
    {
      if(a->size!=b->size)return 0;
      a=a->pre;
      b=b->pre;
    }
  }
  if((a==NULL)&&(b==NULL))return 1;
  else return 0;
}

static int src_compare(src_node *pn1, src_node *pn2, int nt)
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
      if (src_compare((src_node *)(rt1->trace), (src_node *)(rt2->trace), rt1->dist) != 0)
        return -1;
      pn1 = (src_node *)(rt1->pre);
      pn2 = (src_node *)(rt2->pre);
    }
    else
    {
      if (pn1->srcpid != pn2->srcpid)
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

static int src_wrapper_compare(src_trace *st1, src_trace *st2)
{
  if (src_compare(st1->src, st2->src, -1) == 0)
  {
    return 1;
  }
  return 0;
}

static int bytes_wrapper_compare(void *bw1, void *bw2)
{
  bytes_trace *bt1 = (bytes_trace *)bw1;
  bytes_trace *bt2 = (bytes_trace *)bw2;
  if (bytes_compare_func(bt1->bytes, bt2->bytes))
    return 1;
  return 0;
}

static int is_single_p2p_trace(void *trace)
{
  if (trace == NULL) return 0;
  p2p_trace *a = (p2p_trace *)trace;
  if (a->dist < 0) return 0;
  if (a->dist == 1)
  {
    p2p_trace_src *pts = (p2p_trace_src *)a;
    if (pts->pre != NULL) return 0;
  }
  else
  {
    if (a->pre != NULL) return 0;
  }
  return 1;
}

static int is_single_src(src_trace *st)
{
  src_node *sn = st->src;
  if (sn->dist < 0) return 0;
  if (sn->pre != NULL) return 0;
  return 1;
}

static int p2p_bytes_compressable(p2p_trace *target0, p2p_trace *trace0, uint16_t tag, uint32_t pid)
{
  if (target0->dist != 1 || trace0->dist != 1) return 0;
  p2p_trace_src *target = (p2p_trace_src *)target0;
  p2p_trace_src *trace = (p2p_trace_src *)trace0;
  if (target->comm_id != trace->comm_id ||
    target->tag != trace->tag ||
    target->times != trace->times)
    return 0;
  if (!(is_single_src(target->src) && is_single_src(trace->src))) return 0;
  src_node *sn1 = ((src_node *)(((src_trace *)(target->src))->src));
  src_node *sn2 = ((src_node *)(((src_trace *)(trace->src))->src));
  if (tag == 0)
  {
    if (sn1->srcpid != sn2->srcpid || target->dest != trace->dest)
      return 0;
  }
  else if (tag == 1)
  {
    if (sn1->srcpid != sn2->srcpid - pid || target->dest != trace->dest - pid)
      return 0;
  }
  else if (tag == 2)
  {
    if (sn1->srcpid != sn2->srcpid || target->dest != trace->dest - pid)
      return 0;
  }
  else if (tag == 3)
  {
    if (sn1->srcpid != sn2->srcpid - pid || target->dest != trace->dest)
      return 0;
  }
  else
    printf("error: wrong cs tag.\n");
  return 1;
}

static int p2p_bytes_compress(compressed *target, p2p_trace *trace)
{
  proc_stack_add(target, ((bytes_trace *)(trace->bytes))->pid);
  bytes_trace *a = ((p2p_trace *)(target->converted_trace))->bytes;
  bytes_trace *b = bytes_convert(trace->bytes);
  if (b->pid < a->pid)
  {
    b->next = a;
    ((p2p_trace *)(target->converted_trace))->bytes = b;
    return 0;
  }
  while (a->next && a->next->pid < b->pid)
    a = a->next;
  b->next = a->next;
  a->next = b;
  return 0;
}

static int p2p_bytes_merge(p2p_trace_src *pt1, p2p_trace_src *pt2)
{
  bytes_trace *a = pt1->bytes;
  bytes_trace *b = pt2->bytes;
  bytes_trace *head = (bytes_trace *)malloc(sizeof(bytes_trace));
  bytes_trace *now = head;
  while (a && b)
  {
    if (a->pid < b->pid)
    {
      now->next = a;
      a = a->next;
    }
    else
    {
      now->next = b;
      b = b->next;
    }
    now = now->next;
  }
  if (a)
    now->next = a;
  else
    now->next = b;
  now = head->next;
  free(head);
  pt1->bytes = now;
  return 0;
}

static int trace_compare_func0(void *target,void *trace,uint16_t tag, uint32_t pid)
{
  repeat_trace *ra,*rb;
  if(tag==1)
  {
    loop_trace *a,*b;
    a=(loop_trace*)target;
    b=(loop_trace*)trace;
    while((a!=NULL)&&(b!=NULL))
    {
      if(a->dist!=b->dist)return 0;
      if(a->dist==-1)
      {
        ra=(repeat_trace*)a;
        rb=(repeat_trace*)b;
        if((ra->dist!=rb->dist)||(ra->times!=rb->times))return 0;
        if (trace_compare_func0(ra->trace, rb->trace, tag, pid) != 1)
          return 0;
        a=(loop_trace*)ra->pre;
        b=(loop_trace*)rb->pre;
      }
      else
      {
        if((a->times!=b->times)||(a->retimes!=b->retimes))return 0;
        a=a->pre;
        b=b->pre;
      }
    }
    if((a==NULL)&&(b==NULL))return 1;
    else return 0;
  }
  else if(tag==2)
  {
    branch_trace *a,*b;
    a=(branch_trace*)target;
    b=(branch_trace*)trace;
    while((a!=NULL)&&(b!=NULL))
    {
      if(a->dist!=b->dist)return 0;
      if(a->dist==-1)
      {
        ra=(repeat_trace*)a;
        rb=(repeat_trace*)b;
        if((ra->dist!=rb->dist)||(ra->times!=rb->times))return 0;
        if (trace_compare_func0(ra->trace, rb->trace, tag, pid) != 1)
          return 0;
        a=(branch_trace*)ra->pre;
        b=(branch_trace*)rb->pre;
      }
      else
      {
        if((a->torf!=b->torf)||(a->retimes!=b->retimes))return 0;
        a=a->pre;
        b=b->pre;
      }
    }
    if((a==NULL)&&(b==NULL))return 1;
    else return 0;
  }
  else if(tag==5)
  {
    p2p_trace *a,*b;
    a=(p2p_trace*)target;
    b=(p2p_trace*)trace;
    while((a!=NULL)&&(b!=NULL))
    {
      if(a->dist!=b->dist)return 0;
      if(a->dist==-1)
      {
        ra=(repeat_trace*)a;
        rb=(repeat_trace*)b;
        if((ra->dist!=rb->dist)||(ra->times!=rb->times))return 0;
        if (trace_compare_func0(ra->trace, rb->trace, tag, pid) != 1)
          return 0;
        a=(p2p_trace*)ra->pre;
        b=(p2p_trace*)rb->pre;
      }
      else if (a->dist == 1)
      {
        p2p_trace_src *c = (p2p_trace_src *)a;
        p2p_trace_src *d = (p2p_trace_src *)b;
        if((c->comm_id!=d->comm_id)||(c->tag!=d->tag)||(c->dest!=d->dest)||
          src_wrapper_compare(c->src, d->src) == 0 ||
          bytes_wrapper_compare(c->bytes,d->bytes) == 0)
          return 0;
        a=(p2p_trace*)c->pre;
        b=(p2p_trace*)d->pre;
      }
      else{
        if((a->comm_id!=b->comm_id)||(a->src!=b->src)||(a->tag!=b->tag)||(a->dest!=b->dest)||
          bytes_wrapper_compare(a->bytes,b->bytes) == 0)
          return 0;
        a=a->pre;
        b=b->pre;
      }
    }
    if((a==NULL)&&(b==NULL))return 1;
    else return 0;
  }
  else if(tag==6)
  {
    coll_trace *a,*b;
    a=(coll_trace*)target;
    b=(coll_trace*)trace;
    while((a!=NULL)&&(b!=NULL))
    {
      if(a->dist!=b->dist)return 0;
      if(a->dist==-1)
      {
        ra=(repeat_trace*)a;
        rb=(repeat_trace*)b;
        if((ra->dist!=rb->dist)||(ra->times!=rb->times))return 0;
        if (trace_compare_func0(ra->trace, rb->trace, tag, pid) != 1)
          return 0;
        a=(coll_trace*)ra->pre;
        b=(coll_trace*)rb->pre;
      }
      else{
        if((a->comm_id!=b->comm_id)||(a->root_id!=b->root_id)||(a->send_bytes!=b->send_bytes)||(a->recv_bytes!=b->recv_bytes))return 0;
        a=a->pre;
        b=b->pre;
      }
    }
    if((a==NULL)&&(b==NULL))return 1;
    else return 0;
  }
  else if(tag==7)
  {
    //TODO
    return 1;
  }
  else if(tag==8)
  {
    //TODO
    return 1;
  }
  else if(tag==9)
  {
    //TODO
    return 1;
  }
  else printf("error: unknow tag.\n");
  return 0;
}

static void *trace_convert_func0(void *trace,uint16_t tag, uint32_t pid)
{
  //trick func.
  return trace;
}

static int bytes_write_text(bytes_node *lt)
{
  if(lt==NULL)return 0;
  if(lt->dist==-1)
  {
    repeat_trace *rt;
    rt=(repeat_trace*)lt;
    bytes_write_text(rt->pre);
    if (rt->pre)
      fprintf(fp,",");
    fprintf(fp,"%d*<",rt->times);
    bytes_write_text((bytes_node *)(rt->trace));
    fprintf(fp,">");
  }
  else
  {
    bytes_write_text(lt->pre);
    if (lt->pre)
      fprintf(fp,",");
    fprintf(fp,"%d",lt->size);
  }
  return 0;
}

static int src_write_text(src_node *lt)
{
  if(lt==NULL)return 0;
  if(lt->dist==-1)
  {
    repeat_trace *rt;
    rt=(repeat_trace*)lt;
    src_write_text(rt->pre);
    if (rt->pre)
      fprintf(fp,",");
    fprintf(fp,"%d*<",rt->times);
    src_write_text((src_node *)(rt->trace));
    fprintf(fp,">");
  }
  else
  {
    src_write_text(lt->pre);
    if (lt->pre)
      fprintf(fp,",");
    fprintf(fp,"%d",lt->srcpid);
  }
  return 0;
}

static int bytes_wrapper_write_text(bytes_trace *bt)
{
  // merge same wrappers during writing
  uint32_t times = 1;
  while (bt)
  {
    if (bt->next && bytes_compare_func(bt->bytes, bt->next->bytes))
    {
      times++;
    }
    else
    {
      if (times > 1)
      {
        fprintf(fp, "%d*{", times);
        bytes_write_text(bt->bytes);
        fprintf(fp, "}");
      }
      else
      {
        bytes_write_text(bt->bytes);
      }
      if (bt->next)
        fprintf(fp,",");
      times = 1;
    }
    bt = bt->next;
  }
  return 0;
}

static int src_wrapper_write_text(src_trace *bt)
{
  // merge same wrappers during writing
  uint32_t times = 1;
  while (bt)
  {
    if (bt->next && src_compare(bt->src, bt->next->src, -1) == 0)
    {
      times++;
    }
    else
    {
      if (times > 1)
      {
        fprintf(fp, "%d*{", times);
        src_write_text(bt->src);
        fprintf(fp, "}");
      }
      else
      {
        src_write_text(bt->src);
      }
      if (bt->next)
        fprintf(fp,",");
      times = 1;
    }
    bt = bt->next;
  }
  return 0;
}

static int trace_write_text_func0(void *trace,uint16_t tag)
{
  repeat_trace *rt;
  if(tag==1)
  {
    loop_trace *lt;
    lt=(loop_trace*)trace;
    while(lt!=NULL)
    {
      if(lt->dist==-1)
      {
        rt=(repeat_trace*)lt;
        fprintf(fp,"%d*[",rt->times);
        trace_write_text_func0(rt->trace, tag);
        fprintf(fp,"]; ");
        lt=(loop_trace*)rt->pre;
      }
      else{
        fprintf(fp,"%d*%d; ",lt->retimes,lt->times);
        lt=lt->pre;
      }
    }
  }
  else if(tag==2)
  {
    branch_trace *lt;
    lt=(branch_trace*)trace;
    while(lt!=NULL)
    {
      if(lt->dist==-1)
      {
        rt=(repeat_trace*)lt;
        fprintf(fp,"%d*[",rt->times);
        trace_write_text_func0(rt->trace, tag);
        fprintf(fp,"]; ");
        lt=(branch_trace*)rt->pre;
      }
      else{
        char b;
        if(lt->torf==1)b='T';
        else b='F';
        fprintf(fp,"%d*%c; ",lt->retimes,b);
        lt=lt->pre;
      }
    }
  }
  else if(tag==5)
  {
    p2p_trace *lt;
    lt=(p2p_trace*)trace;
    while(lt!=NULL)
    {
      if(lt->dist==-1)
      {
        rt=(repeat_trace*)lt;
        fprintf(fp,"%d*[",rt->times);
        trace_write_text_func0(rt->trace, tag);
        fprintf(fp,"]; ");
        lt=(p2p_trace*)rt->pre;
      }
      else if (lt->dist==1) {
        p2p_trace_src *lts = (p2p_trace_src *)lt;
        fprintf(fp,"comm=%d,src=(",lts->comm_id);
        src_wrapper_write_text(lts->src);
        fprintf(fp, "),tag=%d,dest=%d,times=%d,bytes=(",lts->tag,lts->dest,lts->times);
        bytes_wrapper_write_text(lts->bytes);
        fprintf(fp,"); ");
        lt=(p2p_trace *)(lts->pre);
      }
      else{
        fprintf(fp,"comm=%d,src=%d,tag=%d,dest=%d,times=%d,bytes=(",lt->comm_id,lt->src,lt->tag,lt->dest,lt->times);
        bytes_wrapper_write_text(lt->bytes);
        fprintf(fp,"); ");
        lt=lt->pre;
      }
    }
  }
  else if(tag==6)
  {
    coll_trace *lt;
    lt=(coll_trace*)trace;
    while(lt!=NULL)
    {
      if(lt->dist==-1)
      {
        rt=(repeat_trace*)lt;
        fprintf(fp,"%d*[",rt->times);
        trace_write_text_func0(rt->trace, tag);
        fprintf(fp,"]; ");
        lt=(coll_trace*)rt->pre;
      }
      else{
        fprintf(fp,"comm=%d,root=%d,sendbytes=%d,recvbytes=%d; ",lt->comm_id,lt->root_id,lt->send_bytes,lt->recv_bytes);
        lt=lt->pre;
      }
    }
  }
  //TODO
  else if(tag==7)
  {
    return 0;
  }
  else if(tag==8)
  {
    return 0;
  }
  else if(tag==9)
  {
    return 0;
  }
  else printf("error: unknow tag.\n");
  return 0;
}

static int trace_compare_func1(void *target,void *trace,uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag1 but trace tag not 5.\n");
    return 0;
  }
  p2p_trace *a,*b;
  repeat_trace *ra,*rb;
  a=(p2p_trace*)target;
  b=(p2p_trace*)trace;
  while((a!=NULL)&&(b!=NULL))
  {
    if(a->dist!=b->dist)return 0;
    if(a->dist==-1)
    {
      ra=(repeat_trace*)a;
      rb=(repeat_trace*)b;
      if((ra->dist!=rb->dist)||(ra->times!=rb->times))return 0;
      if (trace_compare_func1(ra->trace, rb->trace, tag, pid) != 1)
          return 0;
      a=(p2p_trace*)ra->pre;
      b=(p2p_trace*)rb->pre;
    }
    else if (a->dist == 1)
    {
      return 0;
    }
    else{
      //src&dest -pid.
      if((a->comm_id!=b->comm_id)||(a->src!=b->src-pid)||(a->tag!=b->tag)||(a->dest!=b->dest-pid)||
        bytes_wrapper_compare(a->bytes,b->bytes) == 0)
        return 0;
      a=a->pre;
      b=b->pre;
    }
  }
  if((a==NULL)&&(b==NULL))return 1;
  else return 0;
}

static int p2p_src_compressable(p2p_trace_src *target, p2p_trace_src *trace)
{
  if (target->comm_id != trace->comm_id ||
    target->tag != trace->tag ||
    target->times != trace->times ||
    target->dest != trace->dest)
    return 0;
  if (bytes_wrapper_compare(target->bytes, trace->bytes) != 1)
    return 0;
  return 1;
}

static int p2p_src_merge(p2p_trace_src *pt1, p2p_trace_src *pt2)
{
  src_trace *a = pt1->src;
  src_trace *b = pt2->src;
  src_trace *head = (src_trace *)malloc(sizeof(src_trace));
  src_trace *now = head;
  while (a && b)
  {
    if (a->pid < b->pid)
    {
      now->next = a;
      a = a->next;
    }
    else
    {
      now->next = b;
      b = b->next;
    }
    now = now->next;
  }
  if (a)
    now->next = a;
  else
    now->next = b;
  now = head->next;
  free(head);
  pt1->src = now;
  return 0;
}

static src_node *src_copy_convert(src_node *a, int delta)
{
  if (a == NULL) return NULL;
  if (a->dist < 0)
  {
    repeat_trace *ra = (repeat_trace *)a;
    repeat_trace *rb = (repeat_trace *)malloc(sizeof(repeat_trace));
    rb->tag = ra->tag;
    rb->dist = ra->dist;
    rb->times = ra->times;
    rb->trace = src_copy_convert((src_node *)(ra->trace), delta);
    rb->pre = src_copy_convert((src_node *)(ra->pre), delta);
    return (src_node *)rb;
  }
  else
  {
    src_node *b = (src_node *)malloc(sizeof(src_node));
    b->dist = a->dist;
    b->srcpid = a->srcpid + delta;
    b->pre = src_copy_convert(a->pre, delta);
    return b;
  }
}

static src_trace *src_convert(src_trace *a, int delta)
{
  src_trace *b = (src_trace *)malloc(sizeof(src_trace));
  b->pid = a->pid;
  b->src = src_copy_convert((src_node *)(a->src), delta);
  b->next = NULL;
  return b;
}

static void *trace_convert_func1(void *trace,uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag1 but trace tag not 5.\n");
    return 0;
  }
  if(trace==NULL)return NULL;
  void *target;
  p2p_trace *a,*b;
  p2p_trace_src *c;
  repeat_trace *ra,*rb;
  target=NULL;
  a=(p2p_trace*)trace;
  if(a->dist==-1)
  {
    ra=(repeat_trace*)a;
    rb=(repeat_trace*)malloc(sizeof(repeat_trace));
    rb->tag=ra->tag;
    rb->dist=ra->dist;
    rb->times=ra->times;
    rb->trace = trace_convert_func1(ra->trace, tag, pid);
    rb->pre=trace_convert_func1(ra->pre,tag, pid);
    target=(void*)rb;
  }
  else if (a->dist == 1) {
    p2p_trace_src *d = (p2p_trace_src*)a;
    c=(p2p_trace_src*)malloc(sizeof(p2p_trace_src));
    c->dist=d->dist;
    c->comm_id=d->comm_id;
    c->src=src_convert(d->src, -pid);
    c->tag=d->tag;
    c->dest=d->dest-pid;
    c->bytes=bytes_convert(d->bytes);
    c->pre=(p2p_trace_src*)trace_convert_func1(d->pre,tag, pid);
    target=(void*)c;
  }
  else
  {
    b=(p2p_trace*)malloc(sizeof(p2p_trace));
    b->dist=a->dist;
    b->comm_id=a->comm_id;
    b->src=a->src-pid;
    b->tag=a->tag;
    b->dest=a->dest-pid;
    b->bytes=bytes_convert(a->bytes);
    b->pre=(p2p_trace*)trace_convert_func1(a->pre,tag, pid);
    target=(void*)b;
  }
  return target;
}

static int trace_write_text_func1(void *trace,uint16_t tag)
{
  if(tag!=5)
  {
    printf("error: cs tag1 but trace tag not 5.\n");
    return 0;
  }

  p2p_trace *lt;
  repeat_trace *rt;
  lt=(p2p_trace*)trace;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      rt=(repeat_trace*)lt;
      fprintf(fp,"%d*[",rt->times);
      trace_write_text_func1(rt->trace, tag);
      fprintf(fp,"]; ");
      lt=(p2p_trace*)rt->pre;
    }
    else if (lt->dist == 1)
    {
      p2p_trace_src *lts = (p2p_trace_src *)lt;
      fprintf(fp,"comm=%d,src=(",lts->comm_id);
      src_wrapper_write_text(lts->src);
      fprintf(fp, ")+proc_id,tag=%d,dest=%d+proc_id,times=%d,bytes=(",lts->tag,lts->dest,lts->times);
      bytes_wrapper_write_text(lts->bytes);
      fprintf(fp,"); ");
      lt=(p2p_trace *)(lts->pre);
    }
    else{
      fprintf(fp,"comm=%d,src=%d+proc_id,tag=%d,dest=%d+proc_id,times=%d,bytes=(",lt->comm_id,lt->src,lt->tag,lt->dest,lt->times);
      bytes_wrapper_write_text(lt->bytes);
      fprintf(fp,"); ");
      lt=lt->pre;
    }
  }
  return 0;
}

static int trace_compare_func2(void *target,void *trace,uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag2 but trace tag not 5.\n");
    return 0;
  }
  p2p_trace *a,*b;
  repeat_trace *ra,*rb;
  a=(p2p_trace*)target;
  b=(p2p_trace*)trace;
  while((a!=NULL)&&(b!=NULL))
  {
    if(a->dist!=b->dist)return 0;
    if(a->dist==-1)
    {
      ra=(repeat_trace*)a;
      rb=(repeat_trace*)b;
      if((ra->dist!=rb->dist)||(ra->times!=rb->times))return 0;
      if (trace_compare_func2(ra->trace, rb->trace, tag, pid) != 1)
          return 0;
      a=(p2p_trace*)ra->pre;
      b=(p2p_trace*)rb->pre;
    }
    else if (a->dist == 1)
    {
      return 0;
    }
    else{
      //only dest -pid.
      if((a->comm_id!=b->comm_id)||(a->src!=b->src)||(a->tag!=b->tag)||(a->dest!=b->dest-pid)||
        bytes_wrapper_compare(a->bytes,b->bytes) == 0)
        return 0;
      a=a->pre;
      b=b->pre;
    }
  }
  if((a==NULL)&&(b==NULL))return 1;
  else return 0;
}

static void *trace_convert_func2(void *trace,uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag2 but trace tag not 5.\n");
    return 0;
  }
  if(trace==NULL)return NULL;
  void *target;
  p2p_trace *a,*b;
  p2p_trace_src *c;
  repeat_trace *ra,*rb;
  target=NULL;
  a=(p2p_trace*)trace;
  if(a->dist==-1)
  {
    ra=(repeat_trace*)a;
    rb=(repeat_trace*)malloc(sizeof(repeat_trace));
    rb->tag=ra->tag;
    rb->dist=ra->dist;
    rb->times=ra->times;
    rb->trace = trace_convert_func2(ra->trace, tag, pid);
    rb->pre=trace_convert_func2(ra->pre,tag, pid);
    target=(void*)rb;
  }
  else if (a->dist == 1) {
    p2p_trace_src *d = (p2p_trace_src*)a;
    c=(p2p_trace_src*)malloc(sizeof(p2p_trace_src));
    c->dist=d->dist;
    c->comm_id=d->comm_id;
    c->src=src_convert(d->src, 0);
    c->tag=d->tag;
    c->dest=d->dest-pid;
    c->bytes=bytes_convert(d->bytes);
    c->pre=(p2p_trace_src*)trace_convert_func1(d->pre,tag, pid);
    target=(void*)c;
  }
  else{
    b=(p2p_trace*)malloc(sizeof(p2p_trace));
    b->dist=a->dist;
    b->comm_id=a->comm_id;
    b->src=a->src;
    b->tag=a->tag;
    b->dest=a->dest-pid;
    b->bytes=bytes_convert(a->bytes);
    b->pre=(p2p_trace*)trace_convert_func2(a->pre,tag, pid);
    target=(void*)b;
  }
  return target;
}

static int trace_write_text_func2(void *trace,uint16_t tag)
{
  if(tag!=5)
  {
    printf("error: cs tag2 but trace tag not 5.\n");
    return 0;
  }
  p2p_trace *lt;
  repeat_trace *rt;
  lt=(p2p_trace*)trace;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      rt=(repeat_trace*)lt;
      fprintf(fp,"%d*[",rt->times);
      trace_write_text_func2(rt->trace, tag);
      fprintf(fp,"]; ");
      lt=(p2p_trace*)rt->pre;
    }
    else if (lt->dist == 1)
    {
      p2p_trace_src *lts = (p2p_trace_src *)lt;
      fprintf(fp,"comm=%d,src=(",lts->comm_id);
      src_wrapper_write_text(lts->src);
      fprintf(fp, "),tag=%d,dest=%d+proc_id,times=%d,bytes=(",lts->tag,lts->dest,lts->times);
      bytes_wrapper_write_text(lts->bytes);
      fprintf(fp,"); ");
      lt=(p2p_trace *)(lts->pre);
    }
    else{
      fprintf(fp,"comm=%d,src=%d,tag=%d,dest=%d+proc_id,times=%d,bytes=(",lt->comm_id,lt->src,lt->tag,lt->dest,lt->times);
      bytes_wrapper_write_text(lt->bytes);
      fprintf(fp,"); ");
      lt=lt->pre;
    }
  }
  return 0;
}

static int trace_compare_func3(void *target,void *trace,uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag3 but trace tag not 5.\n");
    return 0;
  }
  p2p_trace *a,*b;
  repeat_trace *ra,*rb;
  a=(p2p_trace*)target;
  b=(p2p_trace*)trace;
  while((a!=NULL)&&(b!=NULL))
  {
    if(a->dist!=b->dist)return 0;
    if(a->dist==-1)
    {
      ra=(repeat_trace*)a;
      rb=(repeat_trace*)b;
      if((ra->dist!=rb->dist)||(ra->times!=rb->times))return 0;
      if (trace_compare_func3(ra->trace, rb->trace, tag, pid) != 1)
          return 0;
      a=(p2p_trace*)ra->pre;
      b=(p2p_trace*)rb->pre;
    }
    else if (a->dist == 1)
    {
      return 0;
    }
    else{
      //only src -pid.
      if((a->comm_id!=b->comm_id)||(a->src!=b->src-pid)||(a->tag!=b->tag)||(a->dest!=b->dest)||
        bytes_wrapper_compare(a->bytes,b->bytes) == 0)
        return 0;
      a=a->pre;
      b=b->pre;
    }
  }
  if((a==NULL)&&(b==NULL))return 1;
  else return 0;
}

static void *trace_convert_func3(void *trace,uint16_t tag, uint32_t pid)
{
  if(tag!=5)
  {
    printf("error: cs tag3 but trace tag not 5.\n");
    return 0;
  }
  if(trace==NULL)return NULL;
  void *target;
  p2p_trace *a,*b;
  p2p_trace_src *c;
  repeat_trace *ra,*rb;
  target=NULL;
  a=(p2p_trace*)trace;
  if(a->dist==-1)
  {
    ra=(repeat_trace*)a;
    rb=(repeat_trace*)malloc(sizeof(repeat_trace));
    rb->tag=ra->tag;
    rb->dist=ra->dist;
    rb->times=ra->times;
    rb->trace = trace_convert_func3(ra->trace, tag, pid);
    rb->pre=trace_convert_func3(ra->pre,tag, pid);
    target=(void*)rb;
  }
  else if (a->dist == 1) {
    p2p_trace_src *d = (p2p_trace_src*)a;
    c=(p2p_trace_src*)malloc(sizeof(p2p_trace_src));
    c->dist=d->dist;
    c->comm_id=d->comm_id;
    c->src=src_convert(d->src, -pid);
    c->tag=d->tag;
    c->dest=d->dest;
    c->bytes=bytes_convert(d->bytes);
    c->pre=(p2p_trace_src*)trace_convert_func1(d->pre,tag, pid);
    target=(void*)c;
  }
  else{
    b=(p2p_trace*)malloc(sizeof(p2p_trace));
    b->dist=a->dist;
    b->comm_id=a->comm_id;
    b->src=a->src-pid;
    b->tag=a->tag;
    b->dest=a->dest;
    b->bytes=bytes_convert(a->bytes);
    b->pre=(p2p_trace*)trace_convert_func3(a->pre,tag, pid);
    target=(void*)b;
  }
  return target;
}

static int trace_write_text_func3(void *trace,uint16_t tag)
{
  if(tag!=5)
  {
    printf("error: cs tag3 but trace tag not 5.\n");
    return 0;
  }
  p2p_trace *lt;
  repeat_trace *rt;
  lt=(p2p_trace*)trace;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      rt=(repeat_trace*)lt;
      fprintf(fp,"%d*[",rt->times);
      trace_write_text_func0(rt->trace, tag);
      fprintf(fp,"]; ");
      lt=(p2p_trace*)rt->pre;
    }
    else if (lt->dist == 1)
    {
      p2p_trace_src *lts = (p2p_trace_src *)lt;
      fprintf(fp,"comm=%d,src=(",lts->comm_id);
      src_wrapper_write_text(lts->src);
      fprintf(fp, ")+proc_id,tag=%d,dest=%d,times=%d,bytes=(",lts->tag,lts->dest,lts->times);
      bytes_wrapper_write_text(lts->bytes);
      fprintf(fp,"); ");
      lt=(p2p_trace *)(lts->pre);
    }
    else{
      fprintf(fp,"comm=%d,src=%d+proc_id,tag=%d,dest=%d,times=%d,bytes=(",lt->comm_id,lt->src,lt->tag,lt->dest,lt->times);
      bytes_wrapper_write_text(lt->bytes);
      fprintf(fp,"); ");
      lt=lt->pre;
    }
  }
  return 0;
}

// merge 2 to 1, free 2
static void merge_proc_stack(compressed *target1, compressed *target2)
{
  target1->nproc += target2->nproc;
  proc_stack *ps1, *ps2, *ps, *temp;
  ps1 = target1->procs;
  ps2 = target2->procs;
  if (ps1 == NULL || ps2 == NULL)
    printf("error: proc stack is NULL.\n");
  // O(n) merge
  if (ps1->begin_id < ps2->begin_id)
  {
    target1->procs = ps1;
    ps = ps1;
    ps1 = ps1->next;
  }
  else
  {
    target1->procs = ps2;
    ps = ps2;
    ps2 = ps2->next;
  }
  while (ps1 && ps2)
  {
    if (ps1->begin_id < ps2->begin_id)
    {
      if (ps->end_id + 1 == ps1-> begin_id)
      {
        ps->end_id = ps1->end_id;
        temp = ps1->next;
        free(ps1);
        ps1 = temp;
      }
      else
      {
        ps->next = ps1;
        ps = ps1;
        ps1 = ps1->next;
      }
    }
    else
    {
      if (ps->end_id + 1 == ps2-> begin_id)
      {
        ps->end_id = ps2->end_id;
        temp = ps2->next;
        free(ps2);
        ps2 = temp;
      }
      else
      {
        ps->next = ps2;
        ps = ps2;
        ps2 = ps2->next;
      }
    }
  }
  if (ps1)
  {
    if (ps->end_id + 1 == ps1-> begin_id)
    {
      ps->end_id = ps1->end_id;
      ps->next = ps1->next;
      free(ps1);
    }
    else
      ps->next = ps1;
  }
  else if (ps2)
  {
    if (ps->end_id + 1 == ps2-> begin_id)
    {
      ps->end_id = ps2->end_id;
      ps->next = ps2->next;
      free(ps2);
    }
    else
      ps->next = ps2;
  }
}

// work with hash key
static compressed *trace_merge(compressed *head1, compressed *head2, uint16_t tag)
{
  compressed *head = (compressed *)malloc(sizeof(compressed));
  compressed *now = head;
  now->next = NULL;
  while (head1 && head2)
  {
    if (head1->key < head2->key)
    {
      now->next = head1;
      head1 = head1->next;
    }
    else if (head1->key > head2->key)
    {
      now->next = head2;
      head2 = head2->next;
    }
    else
    {
      // if keys are equal, compare O(n^2)
      compressed *ht = head2, *hl = NULL;
      while (ht && ht->key == head1->key)
      {
        // bytes compression
        if (tag == 5)
          if (is_single_p2p_trace(head1->converted_trace) &&
            is_single_p2p_trace(ht->converted_trace))
          {
            if (p2p_src_compressable(head1->converted_trace, ht->converted_trace))
            {
              merge_proc_stack(head1, ht);
              p2p_src_merge(head1->converted_trace, ht->converted_trace);
              if (hl == NULL)
                head2 = head2->next;
              else
                hl->next = ht->next;
              break;
            }
            if (p2p_bytes_compressable(head1->converted_trace, ht->converted_trace, 0, 0))
            {
              merge_proc_stack(head1, ht);
              p2p_bytes_merge(head1->converted_trace, ht->converted_trace);
              if (hl == NULL)
                head2 = head2->next;
              else
                hl->next = ht->next;
              break;
            }
          }

        if (trace_compare_func0(head1->converted_trace, ht->converted_trace, tag, 0) == 1)
        {
          merge_proc_stack(head1, ht);
          if (hl == NULL)
            head2 = head2->next;
          else
            hl->next = ht->next;
          break;
        }
        hl = ht;
        ht = ht->next;
      }
      now->next = head1;
      head1 = head1->next;
    }
    now = now->next;
  }
  if (head1)
    now->next = head1;
  else
    now->next = head2;
  head1 = head->next;
  free(head);
  return head1;
}

static int trace_compress(convert_st *cs,void *trace,uint16_t tag, uint32_t pid)
{
  int n;
  uint32_t tkey;
  compressed *target;
  proc_stack *proc;
  while(cs!=NULL)
  {
    if (cs->tag == 0) tkey = trace_hash_func0(trace, tag, pid);
    else if (cs->tag == 1) tkey = trace_hash_func1(trace, tag, pid);
    else if (cs->tag == 2) tkey = trace_hash_func2(trace, tag, pid);
    else if (cs->tag == 3) tkey = trace_hash_func3(trace, tag, pid);
    else printf("error: wrong convert tag.\n");
    target=cs->ctrace;
    while(target!=NULL)
    {
      printf("possible\n");
      n=0;
      // compare by hash key first
      if (target->key == tkey)
      {
        // for p2p trace, all parameters but bytes are (probably) matched
        // try to perform bytes compression for p2p trace if possible
        if (tag == 5)
        {
          if (is_single_p2p_trace(trace) && is_single_p2p_trace(target->converted_trace))
          {
            // reachable?
            if (target->converted_trace && trace &&
              ((p2p_trace *)(target->converted_trace))->dist == 1 &&
              ((p2p_trace *)trace)->dist == 1)
            {
              printf("reaches!\n");
              //p2p_src_compress(target, trace);
              break;
            }
            if (p2p_bytes_compressable(target->converted_trace, trace, cs->tag, pid))
            {
              p2p_bytes_compress(target, trace);
              break;
            }
          }
        }

        if(cs->tag==0)n=trace_compare_func0(target->converted_trace,trace,tag, pid);
        else if(cs->tag==1)n=trace_compare_func1(target->converted_trace,trace,tag, pid);
        else if(cs->tag==2)n=trace_compare_func2(target->converted_trace,trace,tag, pid);
        else if(cs->tag==3)n=trace_compare_func3(target->converted_trace,trace,tag, pid);
        else printf("error: wrong convert tag.\n");
      }
      if(n==1)
      {
        proc_stack_add(target, pid);
        break;
      }
      target=target->next;
    }
    if(target==NULL)
    {
      target=(compressed*)malloc(sizeof(compressed));
      target->tag=cs->tag;
      target->bol=0;
      target->nproc=1;
      target->key = tkey;
      target->next = NULL;
      proc=(proc_stack*)malloc(sizeof(proc_stack));
      proc->begin_id=pid;
      proc->end_id=pid;
      proc->next=NULL;
      target->procs=proc;
      if(cs->tag==0)target->converted_trace=trace_convert_func0(trace,tag, pid);
      else if(cs->tag==1)target->converted_trace=trace_convert_func1(trace,tag, pid);
      else if(cs->tag==2)target->converted_trace=trace_convert_func2(trace,tag, pid);
      else if(cs->tag==3)target->converted_trace=trace_convert_func3(trace,tag, pid);
      cs->ctrace = trace_insert(target, cs->ctrace);
    }
    if(trace==NULL)return 0;
    cs=cs->next;
  }
  return 0;
}

static void *loop_trace_read(FILE *fp)
{
  int16_t dist;
  loop_trace *lt;
  repeat_trace *rt;
  void *pt;
  pt=NULL;
  fread(&dist,2,1,fp);
  while(dist!=-2)
  {
    if(dist==-1)
    {
      rt=(repeat_trace*)malloc(sizeof(repeat_trace));
      rt->tag=-1;
      fread(&rt->dist,2,1,fp);
      fread(&rt->times,4,1,fp);
      rt->trace = loop_trace_read(fp);
      rt->pre=pt;
      pt=(void*)rt;
    }
    else{
      lt=(loop_trace*)malloc(sizeof(loop_trace));
      lt->dist=dist;
      fread(&lt->times,4,1,fp);
      fread(&lt->retimes,4,1,fp);
      lt->pre=(loop_trace*)pt;
      pt=(void*)lt;
    }
    fread(&dist,2,1,fp);
  }
  return pt;
}

static void *branch_trace_read(FILE *fp)
{
  int16_t dist;
  branch_trace *lt;
  repeat_trace *rt;
  void *pt;
  pt=NULL;
  fread(&dist,2,1,fp);
  while(dist!=-2)
  {
    if(dist==-1)
    {
      rt=(repeat_trace*)malloc(sizeof(repeat_trace));
      rt->tag=-1;
      fread(&rt->dist,2,1,fp);
      fread(&rt->times,4,1,fp);
      rt->trace = branch_trace_read(fp);
      rt->pre=pt;
      pt=(void*)rt;
    }
    else{
      lt=(branch_trace*)malloc(sizeof(branch_trace));
      lt->dist=dist;
      fread(&lt->torf,2,1,fp);
      fread(&lt->retimes,4,1,fp);
      lt->pre=(branch_trace*)pt;
      pt=(void*)lt;
    }
    fread(&dist,2,1,fp);
  }
  return pt;
}

static void *bytes_read(FILE*fp)
{
  int16_t dist;
  bytes_node *lt;
  repeat_trace *rt;
  void *pt;
  pt=NULL;
  fread(&dist,2,1,fp);
  while(dist!=-2)
  {
    if(dist==-1)
    {
      rt=(repeat_trace*)malloc(sizeof(repeat_trace));
      rt->tag=-1;
      fread(&rt->dist,2,1,fp);
      fread(&rt->times,4,1,fp);
      rt->trace = bytes_read(fp);
      rt->pre=pt;
      pt=(void*)rt;
    }
    else{
      lt=(bytes_node*)malloc(sizeof(bytes_node));
      lt->dist=dist;
      fread(&lt->size,4,1,fp);
      lt->pre=(bytes_node*)pt;
      pt=(void*)lt;
    }
    fread(&dist,2,1,fp);
  }
  return pt; 
}

static bytes_trace *bytes_wrapper_read(FILE*fp, uint32_t pid)
{
  bytes_trace *bt = (bytes_trace *)malloc(sizeof(bytes_trace));
  bt->next = NULL;
  bt->bytes = bytes_read(fp);
  bt->pid = pid;
  return bt;
}

static int p2p_src_compressable_intra(void *pt)
{
  if (!pt) return 0;
  // record
  uint16_t comm_id;
  int16_t tag;
  int16_t dest;
  uint32_t times;
  void *bytes;

  p2p_trace *a;
  repeat_trace *r;
  a = (p2p_trace *)pt;
  int flag = 0, level = 1;
  void *stack[10];
  while (level)
  {
    while (a)
    {
      if (a->dist == -1)
      {
        r = (repeat_trace *)a;
        a = (p2p_trace *)r->trace;
        stack[level++] = r->pre;
      }
      else
      {
        if (flag) // first trace has been recorded, compare with it
        {
          if (comm_id != a->comm_id || tag != a->tag || dest != a->dest || times != a->times ||
            bytes_wrapper_compare(bytes, a->bytes) != 1)
          {
            return 0;
          }
        }
        else // record first trace
        {
          flag = 1;
          comm_id = a->comm_id;
          tag = a->tag;
          dest = a->dest;
          times = a->times;
          bytes = a->bytes;
        }
        a = a->pre;
      }
    }
    level--;
    a = (p2p_trace *)stack[level];
  }
  return 1;
}

static src_node *src_free(src_node *pt, int nt)
{
  // free (pt - n, pt], return (pt - n)
  // n == -1 : free all
  repeat_trace *rt;
  src_node *pm;
  while (pt)
  {
    if (nt == 0)
      break;
    nt--;
    if (pt->dist < 0)
    {
      rt = (repeat_trace *)pt;
      src_free((src_node *)(rt->trace), rt->dist);
      pt = (src_node *)(rt->pre);
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

static src_node *src_compress(src_node *pt)
{
  if (pt == NULL)
    return NULL;
  int nt;
  src_node *pt2;
  repeat_trace *rt;
  nt = 1;
  pt2 = pt->dist < 0 ? (src_node *)(((repeat_trace *)pt)->pre) : pt->pre;
  while (pt2)
  {
    if (pt2->dist < 0)
    {
      rt = (repeat_trace *)pt2;
      // try to match (pt - nt, pt] with rt
      if (rt->dist == nt && src_compare((src_node *)(rt->trace), pt, nt) == 0)
      {
        src_free(pt, nt);
        rt->times++;
        // second time compression
        return src_compress(pt2);
      }
    }
    if (src_compare(pt2, pt, nt) == 0)
    {
      rt = (repeat_trace *)malloc(sizeof(repeat_trace));
      rt->tag = -1;
      rt->dist = nt;
      rt->times = 2;
      rt->trace = pt;
      rt->pre = src_free(pt2, nt);
      // set head to NULL, may not be necessary
      src_node *pp = pt;
      int i;
      for (i = 1; i < nt; i++)
        pp = pp->dist < 0 ? (src_node *)(((repeat_trace *)pp)->pre) : pp->pre;
      if (pp->dist < 0)
        ((repeat_trace *)pp)->pre = NULL;
      else
        pp->pre = NULL;
      // second time compression
      return src_compress((src_node *)rt);
    }
    pt2 = pt2->dist < 0 ? (src_node *)(((repeat_trace *)pt2)->pre) : pt2->pre;
    nt++;
  }
  return pt;
}

static void bytes_free(bytes_node *pt)
{
  repeat_trace *rt;
  bytes_node *pm;
  while (pt)
  {
    if (pt->dist < 0)
    {
      rt = (repeat_trace *)pt;
      bytes_free((bytes_node *)(rt->trace));
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
}

static void bytes_wrapper_free(bytes_trace *bt)
{
  bytes_free(bt->bytes);
  free(bt);
}

static void *p2p_src_intra_recompress(p2p_trace *pt, uint32_t pid)
{
  if (!pt) return 0;

  src_trace *st = (src_trace *)malloc(sizeof(src_trace));
  st->pid = pid;
  st->next = NULL;
  st->src = NULL;
  p2p_trace_src *pts = (p2p_trace_src *)malloc(sizeof(p2p_trace_src));
  pts->dist = 1;
  pts->src = st;
  pts->pre = NULL;

  p2p_trace *a, *t;
  a = pt;
  int flag = 0;
  while (a)
  {
    if (flag) // first trace has been recorded
    { 
      if (a->bytes)
      {
        if (pts->bytes) 
          bytes_wrapper_free(a->bytes);
        else
          pts->bytes = a->bytes;
      }
      src_node *sn = (src_node *)malloc(sizeof(src_node));
      sn->dist = 0;
      sn->srcpid = a->src;
      sn->pre = st->src;
      st->src = src_compress(sn);
    }
    else // record first trace
    {
      flag = 1;
      pts->comm_id = a->comm_id;
      pts->tag = a->tag;
      pts->dest = a->dest;
      pts->times = a->times;
      pts->bytes = a->bytes;
      src_node *sn = (src_node *)malloc(sizeof(src_node));
      st->src = sn;
      sn->pre = NULL;
      sn->dist = 0;
      sn->srcpid = a->src;
    }
    t = a;
    a = a->pre;
    free(t);
  }
  return pts;
}

static p2p_trace *p2p_src_intra_duplicate(p2p_trace *b, uint32_t times, p2p_trace **head)
{
  p2p_trace *a = b, *c, *t;
  while (a->pre) a = a->pre;
  *head = a;
  int i;
  p2p_trace **nextPre;
  p2p_trace *lastTail = b;
  p2p_trace *tempTail;
  for (i = 1; i < times; i++)
  {
    nextPre = NULL;
    t = b;
    c = (p2p_trace *)malloc(sizeof(p2p_trace));
    tempTail = c;
    nextPre = &(c->pre);
    *c = *t;
    c->bytes = NULL; // needless to copy
    t = t->pre;
    while (t)
    {
      c = (p2p_trace *)malloc(sizeof(p2p_trace));
      if (nextPre)
        *nextPre = c;
      nextPre = &(c->pre);
      *c = *t;
      c->bytes = NULL; // needless to copy
      t = t->pre;
    }
    *nextPre = lastTail;
    lastTail = tempTail;
  }
  return lastTail;
}

static p2p_trace *p2p_src_intra_decompress(void *pt)
{
  if (!pt) return 0;
  p2p_trace *a, *b;
  repeat_trace *r;
  a = (p2p_trace *)pt;
  void *ret = NULL;
  while (a)
  {
    if (a->dist == -1)
    {
      r = (repeat_trace *)a;
      p2p_trace *head;
      b = p2p_src_intra_decompress(r->trace);
      b = p2p_src_intra_duplicate(b, r->times, &head);
      head->pre = ret;
      ret = b;
      a = r->pre;
      free(r);
    }
    else
    {
      b = a;
      a = a->pre;
      b->pre = ret;
      ret = b;
    }
  }
  return ret;
}

static void *p2p_trace_read(FILE *fp, uint32_t pid)
{
  int16_t dist;
  p2p_trace *lt;
  repeat_trace *rt;
  void *pt;
  pt=NULL;
  fread(&dist,2,1,fp);
  while(dist!=-2)
  {
    if(dist==-1)
    {
      rt=(repeat_trace*)malloc(sizeof(repeat_trace));
      rt->tag=-1;
      fread(&rt->dist,2,1,fp);
      fread(&rt->times,4,1,fp);
      rt->trace = p2p_trace_read(fp, pid);
      rt->pre=pt;
      pt=(void*)rt;
    }
    else{
      lt=(p2p_trace*)malloc(sizeof(p2p_trace));
      lt->dist=dist;
      fread(&lt->comm_id,2,1,fp);
      fread(&lt->src,2,1,fp);
      fread(&lt->tag,2,1,fp);
      fread(&lt->dest,2,1,fp);
      fread(&lt->times,4,1,fp);
      lt->bytes = bytes_wrapper_read(fp, pid);
      lt->pre=(p2p_trace*)pt;
      pt=(void*)lt;
    }
    fread(&dist,2,1,fp);
  }

  return pt;
}

static void *coll_trace_read(FILE *fp)
{
  int16_t dist;
  coll_trace *lt;
  repeat_trace *rt;
  void *pt;
  pt=NULL;
  fread(&dist,2,1,fp);
  while(dist!=-2)
  {
    if(dist==-1)
    {
      rt=(repeat_trace*)malloc(sizeof(repeat_trace));
      rt->tag=-1;
      fread(&rt->dist,2,1,fp);
      fread(&rt->times,4,1,fp);
      rt->trace = coll_trace_read(fp);
      rt->pre=pt;
      pt=(void*)rt;
    }
    else{
      lt=(coll_trace*)malloc(sizeof(coll_trace));
      lt->dist=dist;
      fread(&lt->comm_id,2,1,fp);
      fread(&lt->root_id,2,1,fp);
      fread(&lt->send_bytes,4,1,fp);
      fread(&lt->recv_bytes,4,1,fp);
      lt->pre=(coll_trace*)pt;
      pt=(void*)lt;
    }
    fread(&dist,2,1,fp);
  }
  return pt;
}

static node_st2 *trace_struct_read(node_st2 *nst,int ns,int nn, uint32_t pid, FILE *fp)
{
  int n;
  uint16_t tag,id,other_id;
  uint32_t times,nchild,other_nchild;
  node_st2 *tst;
  statis_info *si;
  node_info *ti;
  for(n=0;n<ns;n++)
  {
    fread(&tag,2,1,fp);
    fread(&id,2,1,fp);
    fread(&times,4,1,fp);
    if(tag==1)
    {
      fread(&nchild,4,1,fp);
      if(nst->next==NULL)
      {
        nst->next=new_node_st2(tag,id,nchild);
        nst=nst->next;
        nst->other=(node_st2*)malloc(sizeof(node_st2));
        nst->other->next=NULL;
        nst->other->other=NULL;
      }
      else{
        nst=nst->next;
        if((nst->tag!=1)||(nst->id!=id)||(nst->nchild!=nchild))printf("error: loop node not match.\n");
      }
      ti=nst->info;
      ti->times=times;
      si=nst->sinfo;
      if(si->max_times<times)si->max_times=times;
      if((si->min_times>times)||(si->min_times<0))si->min_times=times;
      si->total_times=si->total_times+times;
      nst->trace=loop_trace_read(fp);
      trace_compress(nst->fcs,nst->trace,nst->tag, pid);
      tst=trace_struct_read(nst,nst->nchild,nn, pid, fp);
      if(tst->next==NULL)tst->next=nst;
      else if(tst->next!=nst)printf("error: loop exit not match.\n");
      nst=nst->other;
    }
    else if(tag==2)
    {
      fread(&nchild,4,1,fp);
      fread(&other_id,2,1,fp);
      fread(&other_nchild,4,1,fp);
      if(nst->next==NULL)
      {
        nst->next=new_node_st2(tag,id,nchild);
        nst=nst->next;
        nst->other=(node_st2*)malloc(sizeof(node_st2));
        nst->other->id=other_id;
        nst->other->nchild=other_nchild;
        nst->other->next=NULL;
        nst->other->other=NULL;
      }
      else{
        nst=nst->next;
        if((nst->tag!=2)||(nst->id!=id)||(nst->nchild!=nchild))printf("error: branch node not match.\n");
        tst=nst->other;
        if((tst==NULL)||(tst->id!=other_id)||(tst->nchild!=other_nchild))printf("error: branch else node not match.\n");
      }
      ti=nst->info;
      ti->times=times;
      si=nst->sinfo;
      if(si->max_times<times)si->max_times=times;
      if((si->min_times>times)||(si->min_times<0))si->min_times=times;
      si->total_times=si->total_times+times;
      nst->trace=branch_trace_read(fp);
      trace_compress(nst->fcs,nst->trace,nst->tag, pid);
      tst=trace_struct_read(nst,nst->nchild,nn, pid, fp);
      //else has no terminal in inter. for no mask.
      trace_struct_read(nst->other,nst->other->nchild,nn, pid, fp);
      nst=tst;
    }
    else if((tag>4)&&(tag<10))
    {
      if(nst->next==NULL)
      {
        nst->next=new_node_st2(tag,id,0);
        nst=nst->next;
      }
      else{
        nst=nst->next;
        if((nst->tag!=tag)||(nst->id!=id))printf("error: tag%d node not match.\n",tag);
      }
      si=nst->sinfo;
      if(si->max_times<times)si->max_times=times;
      if((si->min_times>times)||(si->min_times<0))si->min_times=times;
      si->total_times=si->total_times+times;
      ti=nst->info;
      ti->times=times;
      if(tag<9)
      {
        fread(&ti->sum_time,sizeof(double),1,fp);
        fread(&ti->sum_sqr_time,sizeof(double),1,fp);
        if(si->max_time<ti->sum_time)si->max_time=ti->sum_time;
        if((si->min_time>ti->sum_time)||(si->min_time<0))si->min_time=ti->sum_time;
        si->total_time=si->total_time+ti->sum_time;
        si->total_sqr_time=si->total_sqr_time+ti->sum_sqr_time;
      }
      if(tag==5) {
        nst->trace=p2p_trace_read(fp, pid);
        // src intra-node compress
        if (p2p_src_compressable_intra(nst->trace))
        {
          nst->trace = p2p_src_intra_decompress(nst->trace);
          nst->trace = p2p_src_intra_recompress(nst->trace, pid);
        }
      }
      else if(tag==6)nst->trace=coll_trace_read(fp);
      else nst->trace=NULL;
      //TODO
      //else if(tag==7)nst->trace=wait_trace_read(fp);
      //else if(tag==8)nst->trace=waitall_trace_read(fp);
      //else if(tag==9)nst->trace=comm_trace_read(fp);
      trace_compress(nst->fcs,nst->trace,nst->tag, pid);
    }
    else printf("error: wrong tag.\n");
  }
  return nst;
}

static node_st2 *intra_trace_read(node_st2 *nst, int nn, FILE *fp)
{
  uint32_t pid;
  uint32_t nchild;
  fread(&pid,4,1,fp);
  fread(&nchild,4,1,fp);
  proc_list[nn]=pid;
  if(nst==NULL)
  {
    nst=(node_st2*)malloc(sizeof(node_st2));
    nst->tag=0;
    nst->id=0;
    nst->nchild=nchild;
    nst->sinfo=NULL;
    nst->info=NULL;
    nst->trace=NULL;
    nst->fcs=NULL;
    nst->next=NULL;
    nst->other=NULL;
  }
  if(nchild!=nst->nchild)printf("error: head not match.\n");
  trace_struct_read(nst,nchild,nn, pid, fp);
  return nst;
}

static compressed *find_major(convert_st *cs,int bol)
{
  int n;
  if(np<9)n=2;
  else n=3;
  compressed *mc,*nc;
  mc=NULL;
  while(cs!=NULL)
  {
    nc=cs->ctrace;
    while(nc!=NULL)
    {
      if((bol==0)||(nc->bol==0))
        if((nc->nproc>=n)&&((mc==NULL)||(mc->nproc<nc->nproc)))mc=nc;
      nc=nc->next;
    }
    cs=cs->next;
  }
  return mc;
}

static compressed *find_basic(convert_st *cs,uint32_t pid)
{
  compressed *nc;
  proc_stack *ps;
  if(cs->tag!=0)printf("error: wrong basic arg cs tag.\n");
  nc=cs->ctrace;
  while(nc!=NULL)
  {
    ps=nc->procs;
    while((ps!=NULL)&&(ps->end_id<pid))ps=ps->next;
    if((ps!=NULL)&&(ps->begin_id<=pid))return nc;
    nc=nc->next;
  }
  return NULL;
}

static int final_trace_write_text(convert_st *fcs,uint16_t tag)
{
  int n;
  proc_node *proc_use;
  compressed *tc;
  proc_stack *ps;
  proc_use=(proc_node*)malloc(np*sizeof(proc_node));
  for(n=0;n<np;n++)
  {
    proc_use[n].pid=proc_list[n];
    proc_use[n].bol=0;
  }
  tc=find_major(fcs,0);
  while(tc!=NULL)
  {
    tc->nproc=0;
    fprintf(fp,"(");
    ps=tc->procs;
    while(ps!=NULL)
    {
      for(n=0;n<np;n++)
        if((proc_use[n].pid>=ps->begin_id)&&(proc_use[n].pid<=ps->end_id))proc_use[n].bol=1;
      if(ps->end_id>ps->begin_id+1)fprintf(fp,"%d-%d;",ps->begin_id,ps->end_id);
      else if(ps->begin_id==ps->end_id)fprintf(fp,"%d;",ps->begin_id);
      else fprintf(fp,"%d;%d;",ps->begin_id,ps->end_id);
      ps=ps->next;
    }
    fprintf(fp,"){");
    if(tc->tag==0)trace_write_text_func0(tc->converted_trace,tag);
    else if(tc->tag==1)trace_write_text_func1(tc->converted_trace,tag);
    else if(tc->tag==2)trace_write_text_func2(tc->converted_trace,tag);
    else if(tc->tag==3)trace_write_text_func3(tc->converted_trace,tag);
    else printf("error: unknow cs tag.\n");
    fprintf(fp,"}; ");
    tc=find_major(fcs,0);
  }
  for(n=0;n<np;n++)
    if(proc_use[n].bol==0)
    {
      tc=find_basic(fcs,proc_use[n].pid);
      if(tc==NULL)printf("error: basic trace not found.\n");
      proc_use[n].bol=1;
      fprintf(fp,"(%d){",proc_use[n].pid);
      trace_write_text_func0(tc->converted_trace,tag);
      fprintf(fp,"}; ");
    }
  free(proc_use);
  return 0;
}

static node_st2 *final_write_text(node_st2 *nst,int nt,int nb)
{
  int n;
  statis_info *si;
  while(nt>0)
  {
    if(nst==NULL)
    {
      printf("error: text write null node.\n");
      return NULL;
    }
    si=nst->sinfo;
    for(n=0;n<nb;n++)fprintf(fp,"| ");
    fprintf(fp,"|-");
    if(nst->tag==1)fprintf(fp,"loop id=%d, max_times=%d, min_times=%d, total_times=%d\n",nst->id,si->max_times,si->min_times,si->total_times);
    else if(nst->tag==2)fprintf(fp,"branch if id=%d, else id=%d, max_times=%d, min_times=%d, total_times=%d\n",
          nst->id,nst->other->id,si->max_times,si->min_times,si->total_times);
    else if(nst->tag==5)fprintf(fp,"p2p id=%d, max_times=%d, min_times=%d, total_times=%d, max_time=%.0f, min_time=%.0f, total_time=%.0f, total_sqr_time=%.0f\n",
          nst->id,si->max_times,si->min_times,si->total_times,si->max_time,si->min_time,si->total_time,si->total_sqr_time);
    else if(nst->tag==6)fprintf(fp,"coll id=%d, max_times=%d, min_times=%d, total_times=%d, max_time=%.0f, min_time=%.0f, total_time=%.0f, total_sqr_time=%.0f\n",
          nst->id,si->max_times,si->min_times,si->total_times,si->max_time,si->min_time,si->total_time,si->total_sqr_time);
    else if(nst->tag==7)fprintf(fp,"wait id=%d, max_times=%d, min_times=%d, total_times=%d, max_time=%.0f, min_time=%.0f, total_time=%.0f, total_sqr_time=%.0f\n",
          nst->id,si->max_times,si->min_times,si->total_times,si->max_time,si->min_time,si->total_time,si->total_sqr_time);
    else if(nst->tag==8)fprintf(fp,"waitall id=%d, max_times=%d, min_times=%d, total_times=%d, max_time=%.0f, min_time=%.0f, total_time=%.0f, total_sqr_time=%.0f\n",
          nst->id,si->max_times,si->min_times,si->total_times,si->max_time,si->min_time,si->total_time,si->total_sqr_time);
    else if(nst->tag==9)fprintf(fp,"comm id=%d, max_times=%d, min_times=%d, total times=%d\n",nst->id,si->max_times,si->min_times,si->total_times);
    else printf("error: unknow tag.\n");
    for(n=0;n<nb+1;n++)fprintf(fp,"| ");
    final_trace_write_text(nst->fcs,nst->tag);
    fprintf(fp,"\n");
    if(nst->tag==2)
    {
      node_st2 *tst;
      tst=final_write_text(nst->next,nst->nchild,nb+1);
      for(n=0;n<nb;n++)fprintf(fp,"| ");
      fprintf(fp,"|-");
      fprintf(fp,"else id=%d\n",nst->other->id);
      final_write_text(nst->other->next,nst->other->nchild,nb+1);
      nst=tst;
    }
    else{
      if(nst->tag==1)
      {
        if(nst!=final_write_text(nst->next,nst->nchild,nb+1))printf("error: text loop write error.\n");
        nst=nst->other;
      }
      nst=nst->next;
    }
    nt--;
  }
  return nst;
}

static int repeat_trace_write_binary(repeat_trace *lt)
{
  fwrite(&lt->tag,2,1,fp);
  fwrite(&lt->dist,2,1,fp);
  fwrite(&lt->times,4,1,fp);
  return 0;
}

static int bytes_write_binary(bytes_node *lt)
{
  int16_t n;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      repeat_trace *rt;
      rt=(repeat_trace*)lt;
      repeat_trace_write_binary(rt);
      bytes_write_binary((bytes_node *)(rt->trace));
      lt=(bytes_node*)rt->pre;
    }
    else{
      fwrite(&lt->dist,2,1,fp);
      fwrite(&lt->size,4,1,fp);
      lt=lt->pre;
    }
  }
  n=-2;
  fwrite(&n,2,1,fp);
  return 0;
}

static int src_write_binary(src_node *lt)
{
  int16_t n;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      repeat_trace *rt;
      rt=(repeat_trace*)lt;
      repeat_trace_write_binary(rt);
      src_write_binary((src_node *)(rt->trace));
      lt=(src_node*)rt->pre;
    }
    else{
      fwrite(&lt->dist,2,1,fp);
      fwrite(&lt->srcpid,4,1,fp);
      lt=lt->pre;
    }
  }
  n=-2;
  fwrite(&n,2,1,fp);
  return 0;
}

static int bytes_wrapper_write_binary(bytes_trace *bt)
{
  // merge same wrappers during writing
  uint32_t times = 1;
  uint8_t merged;
  while (bt)
  {
    if (bt->next && bytes_compare_func(bt->bytes, bt->next->bytes))
    {
      times++;
    }
    else
    {
      if (times > 1)
      {
        merged = 1;
        fwrite(&merged,1,1,fp);
        fwrite(&times,4,1,fp);
      }
      else
      {
        merged = 0;
        fwrite(&merged,1,1,fp);
      }
      bytes_write_binary(bt->bytes);
      times = 1;
    }
    bt = bt->next;
  }
  return 0;
}

static int src_wrapper_write_binary(src_trace *bt)
{
  // merge same wrappers during writing
  uint32_t times = 1;
  uint8_t merged;
  while (bt)
  {
    if (bt->next && src_compare(bt->src, bt->next->src, -1) == 0)
    {
      times++;
    }
    else
    {
      if (times > 1)
      {
        merged = 1;
        fwrite(&merged,1,1,fp);
        fwrite(&times,4,1,fp);
      }
      else
      {
        merged = 0;
        fwrite(&merged,1,1,fp);
      }
      src_write_binary(bt->src);
      times = 1;
    }
    bt = bt->next;
  }
  return 0;
}

static int trace_write_binary(void *trace,uint16_t tag)
{
  int16_t n;
  repeat_trace *rt;
  if(tag==1)
  {
    loop_trace *lt;
    lt=(loop_trace*)trace;
    while(lt!=NULL)
    {
      if(lt->dist==-1)
      {
        rt=(repeat_trace*)lt;
        repeat_trace_write_binary(rt);
        trace_write_binary(rt->trace, tag);
        lt=(loop_trace*)rt->pre;
      }
      else{
        fwrite(&lt->dist,2,1,fp);
        fwrite(&lt->times,4,1,fp);
        fwrite(&lt->retimes,4,1,fp);
        lt=lt->pre;
      }
    }
  }
  else if(tag==2)
  {
    branch_trace *lt;
    lt=(branch_trace*)trace;
    while(lt!=NULL)
    {
      if(lt->dist==-1)
      {
        rt=(repeat_trace*)lt;
        repeat_trace_write_binary(rt);
        trace_write_binary(rt->trace, tag);
        lt=(branch_trace*)rt->pre;
      }
      else{
        fwrite(&lt->dist,2,1,fp);
        fwrite(&lt->torf,2,1,fp);
        fwrite(&lt->retimes,4,1,fp);
        lt=lt->pre;
      }
    }
  }
  else if(tag==5)
  {
    p2p_trace *lt;
    lt=(p2p_trace*)trace;
    while(lt!=NULL)
    {
      if(lt->dist==-1)
      {
        rt=(repeat_trace*)lt;
        repeat_trace_write_binary(rt);
        trace_write_binary(rt->trace, tag);
        lt=(p2p_trace*)rt->pre;
      }
      else if (lt->dist == 1) {
        p2p_trace_src *lts = (p2p_trace_src *)lt;
        fwrite(&lts->dist,2,1,fp);
        fwrite(&lts->comm_id,2,1,fp);
        //fwrite(&lts->src,2,1,fp);
        src_wrapper_write_binary(lts->src);
        fwrite(&lts->tag,2,1,fp);
        fwrite(&lts->dest,2,1,fp);
        fwrite(&lts->times,4,1,fp);
        bytes_wrapper_write_binary(lts->bytes);
        lt=(p2p_trace*)lts->pre;
      }
      else{
        fwrite(&lt->dist,2,1,fp);
        fwrite(&lt->comm_id,2,1,fp);
        fwrite(&lt->src,2,1,fp);
        fwrite(&lt->tag,2,1,fp);
        fwrite(&lt->dest,2,1,fp);
        fwrite(&lt->times,4,1,fp);
        bytes_wrapper_write_binary(lt->bytes);
      lt=lt->pre;
      }
    }
  }
  else if(tag==6)
  {
    coll_trace *lt;
    lt=(coll_trace*)trace;
    while(lt!=NULL)
    {
      if(lt->dist==-1)
      {
        rt=(repeat_trace*)lt;
        repeat_trace_write_binary(rt);
        trace_write_binary(rt->trace, tag);
        lt=(coll_trace*)rt->pre;
      }
      else{
        fwrite(&lt->dist,2,1,fp);
        fwrite(&lt->comm_id,2,1,fp);
        fwrite(&lt->root_id,2,1,fp);
        fwrite(&lt->send_bytes,4,1,fp);
        fwrite(&lt->recv_bytes,4,1,fp);
        lt=lt->pre;
      }
    }
  }
  //TODO
  else if(tag==7)
  {
  }
  else if(tag==8)
  {
  }
  else if(tag==9)
  {
  }
  n=-2;
  fwrite(&n,2,1,fp);
  return 0;
}

static int final_trace_write_binary(convert_st *fcs,uint16_t tag)
{
  int n;
  int16_t t;
  uint32_t u;
  proc_node *proc_use;
  compressed *tc;
  proc_stack *ps;
  proc_use=(proc_node*)malloc(np*sizeof(proc_node));
  for(n=0;n<np;n++)
  {
    proc_use[n].pid=proc_list[n];
    proc_use[n].bol=0;
  }
  tc=find_major(fcs,1);
  while(tc!=NULL)
  {
    tc->bol=1;
    fwrite(&tc->tag,2,1,fp);
    fwrite(&tc->nproc,4,1,fp);
    ps=tc->procs;
    while(ps!=NULL)
    {
      for(n=0;n<np;n++)
        if((proc_use[n].pid>=ps->begin_id)&&(proc_use[n].pid<=ps->end_id))proc_use[n].bol=1;
      fwrite(&ps->begin_id,4,1,fp);
      fwrite(&ps->end_id,4,1,fp);
      ps=ps->next;
    }
    trace_write_binary(tc->converted_trace,tag);
    tc=find_major(fcs,1);
  }
  for(n=0;n<np;n++)
    if(proc_use[n].bol==0)
    {
      tc=find_basic(fcs,proc_use[n].pid);
      if(tc==NULL)printf("error: basic trace not found.\n");
      proc_use[n].bol=1;
      t=0;
      u=1;
      fwrite(&t,2,1,fp);
      fwrite(&u,4,1,fp);
      fwrite(&proc_use[n].pid,4,1,fp);
      trace_write_binary(tc->converted_trace,tag);
    }
  free(proc_use);
  t=-1;
  fwrite(&t,2,1,fp);
  return 0;
}

static node_st2 *final_write_binary(node_st2 *nst,int nt)
{
  int n;
  for(;nt>0;nt--)
  {
    if(nst==NULL)
    {
      printf("error: binary write null node.\n");
      return NULL;
    }
    if(nst->tag>0)
    {
      fwrite(&nst->tag,2,1,fp);
      fwrite(&nst->id,2,1,fp);
      //for(n=0;n<np;n++)fwrite(&nst->info[n].times,4,1,fp);
      fwrite(&nst->sinfo->max_times,4,1,fp);
      fwrite(&nst->sinfo->min_times,4,1,fp);
      fwrite(&nst->sinfo->total_times,4,1,fp);
      if(nst->tag==1)
      {
        fwrite(&nst->nchild,4,1,fp);
        final_trace_write_binary(nst->fcs,nst->tag);
        if(nst!=final_write_binary(nst->next,nst->nchild))printf("error: binary loop write error.\n");
        nst=nst->other;
      }
      else if(nst->tag==2)
      {
        node_st2 *tst;
        fwrite(&nst->nchild,4,1,fp);
        fwrite(&nst->other->id,2,1,fp);
        fwrite(&nst->other->nchild,4,1,fp);
        final_trace_write_binary(nst->fcs,nst->tag);
        tst=final_write_binary(nst->next,nst->nchild);
        final_write_binary(nst->other->next,nst->other->nchild);
        nst=tst;
        continue;
      }
      else if(nst->tag==9)
      {
        final_trace_write_binary(nst->fcs,nst->tag);
      }
      else{
        //for(n=0;n<np;n++)fwrite(&nst->info[n].sum_time,sizeof(double),1,fp);
        //for(n=0;n<np;n++)fwrite(&nst->info[n].sum_sqr_time,sizeof(double),1,fp);
        fwrite(&nst->sinfo->max_time,sizeof(double),1,fp);
        fwrite(&nst->sinfo->min_time,sizeof(double),1,fp);
        fwrite(&nst->sinfo->total_time,sizeof(double),1,fp);
        fwrite(&nst->sinfo->total_sqr_time,sizeof(double),1,fp);
        final_trace_write_binary(nst->fcs,nst->tag);
      }
    }
    else{
      fwrite(&np,4,1,fp);
      for(n=0;n<np;n++)fwrite(&proc_list[n],4,1,fp);
      fwrite(&nst->nchild,4,1,fp);
    }
    nst=nst->next;
  }
  return nst;
}

static void merge_statis_info(statis_info *si1, statis_info *si2, uint16_t tag)
{
  if (si1->max_times < si2->max_times)
    si1->max_times = si2->max_times;
  if (si1->min_times > si2->min_times || si1->min_times < 0)
    si1->min_times = si2->min_times;
  si1->total_times += si2->total_times;
  if (tag > 4 && tag < 9)
  {
    if (si1->max_time < si2->max_time)
      si1->max_time = si2->max_time;
    if (si1->min_time > si2->min_time || si1->min_time < 0)
      si1->min_time = si2->min_time;
    si1->total_time += si2->total_time;
    si1->total_sqr_time += si2->total_sqr_time;
  }
}

static void merge_convert_st(convert_st *cs1, convert_st *cs2, uint16_t tag)
{
  while (cs1)
  {
    cs1->ctrace = trace_merge(cs1->ctrace, cs2->ctrace, tag);
    cs1 = cs1->next;
    cs2 = cs2->next;
  }
}

static void trace_struct_merge(node_st2 **p_root_st1, node_st2 **p_root_st2, int ns)
{
  int n;
  uint16_t tag;
  node_st2 *tst1, *tst2, *nst1, *nst2;
  nst1 = *p_root_st1;
  nst2 = *p_root_st2;

  for(n = 0; n < ns; ++n)
  {
    nst1 = nst1->next;
    nst2 = nst2->next;
    if (nst1->id != nst2->id)
      printf("error: trace struct not match.\n");

    tag = nst1->tag;
    merge_statis_info(nst1->sinfo, nst2->sinfo, tag);
    merge_convert_st(nst1->fcs, nst2->fcs, tag);

    if (tag == 1)
    {
      tst1 = nst1;
      tst2 = nst2;
      trace_struct_merge(&tst1, &tst2, nst1->nchild);
      if (tst1->next != nst1 || tst2->next != nst2)
        printf("error: loop exit not match.\n");
      nst1 = nst1->other;
      nst2 = nst2->other;
      // break the loop of tree2 to make it easier to be freed
      tst2->next = NULL;
    }
    else if (tag == 2)
    {
      tst1 = nst1->other;
      tst2 = nst2->other;
      trace_struct_merge(&nst1, &nst2, nst1->nchild);
      // else has no terminal in inter. for no mask.
      trace_struct_merge(&tst1, &tst2, tst1->nchild);
    }
    else if ((tag > 4) && (tag < 10)) {}
    else
      printf("error: wrong tag.\n");
  }

  *p_root_st1 = nst1;
  *p_root_st2 = nst2;
}

// static void free_tree(node_st2 *root_st)
// {
//   if (root_st == NULL)
//     printf("error: free NULL node.\n");
//   if (root_st->other)
//     free_tree(root_st->other);
//   if (root_st->next)
//     free_tree(root_st->next);

//   free(root_st->sinfo);
//   free(root_st->info);

//   uint16_t tag = root_st->tag;
//   void *trace = root_st->trace;
//   while (trace)
//   {
//     if (tag == 1)
//   }

//   free(root_st->fcs);

//   free(root_st);
// }

static void *merge_tree(void *args)
{
  node_st2 **roots = (node_st2 **)args;
  node_st2 *root_st1 = roots[0];
  node_st2 *root_st2 = roots[1];

  // tree2 is merged to tree1
  trace_struct_merge(&root_st1, &root_st2, root_st1->nchild);

  // clean up tree2
  // free_tree(roots[1]);

  if (pthread_mutex_lock(&qmutex) != 0)
  {
    printf("error: mutex lock failed\n");
    return NULL;
  }
  root_queue[tail] = roots[0];
  tail = (tail + 1) % max_queue_size;
  merging_thread_count--;
  if (pthread_mutex_unlock(&qmutex) != 0)
  {
    printf("error: mutex unlock failed\n");
    return NULL;
  }
  return NULL;
}

static void *build_tree(void *args)
{
  int n;
  node_st2 *root_st;
  n = *((int *)args);

  root_st = NULL;
  FILE *f;
  f = fopen(argvs[n], "r");
  if (f == NULL)
  {
    printf("error: wrong arg \"%s\"\n", argvs[n]);
    return NULL;
  }
  root_st = intra_trace_read(root_st, n - 1, f);
  fclose(f);

  if (pthread_mutex_lock(&qmutex) != 0)
  {
    printf("error: mutex lock failed\n");
    return NULL;
  }
  root_queue[tail] = root_st;
  tail = (tail + 1) % max_queue_size;
  building_thread_count--;
  if (pthread_mutex_unlock(&qmutex) != 0)
  {
    printf("error: mutex unlock failed\n");
    return NULL;
  }
  return NULL;
}

static int create_tree_buiding_threads(int *args, int argc, pthread_t *threads, int tc)
{
  int n, err;
  for (n = 1; n < argc; ++n)
  {
    args[n] = n;

    if (pthread_mutex_lock(&qmutex) != 0)
    {
      printf("error: mutex lock failed\n");
      return -1;
    }

    if (building_thread_count < max_building_thread_count)
    {
      building_thread_count++;
      if (pthread_mutex_unlock(&qmutex) != 0)
      {
        printf("error: mutex unlock failed\n");
        return -1;
      }

      err = pthread_create(&threads[tc], NULL, build_tree, &args[n]);
      tc++;
      if (err != 0)
      {
        printf("error: can't create thread.\n");
        return -1;
      }
    }
    else
    {
      if (pthread_mutex_unlock(&qmutex) != 0)
      {
        printf("error: mutex unlock failed\n");
        return -1;
      }
      n--;
    }
  }
  return 0;
}

static int create_tree_merging_threads(node_st2 **roots, pthread_t *threads, int tc)
{
  int p = 0, err;
  while (1)
  {
    if (pthread_mutex_lock(&qmutex) != 0)
    {
      printf("error: mutex lock failed\n");
      return -1;
    }
    if ((head + 1) % max_queue_size == tail)
    {
      if (building_thread_count + merging_thread_count == 0)
        break;
    }
    else if (head != tail && merging_thread_count < max_merging_thread_count)
    {
      roots[p] = root_queue[head];
      head = (head + 1) % max_queue_size;
      roots[p + 1] = root_queue[head];
      head = (head + 1) % max_queue_size;

      merging_thread_count++;
      err = pthread_create(&threads[tc], NULL, merge_tree, &roots[p]);
      tc++;
      p += 2;
      if (err != 0)
      {
        printf("error: can't create thread.\n");
        return -1;
      }
    }
    if (pthread_mutex_unlock(&qmutex) != 0)
    {
      printf("error: mutex unlock failed\n");
      return -1;
    }
  }
  return 0;
}

static int parse_parameters(int argc,char **argv)
{
  if (argc < 4)
    return -1;
  if (strlen(argv[argc - 2]) < 15 ||
      memcmp(argv[argc - 2], "--read-thread=", 14))
    return -1;
  max_building_thread_count = atoi(argv[argc - 2] + 14);
  if (strlen(argv[argc - 1]) < 16 ||
      memcmp(argv[argc - 1], "--merge-thread=", 15))
    return -1;
  max_merging_thread_count = atoi(argv[argc - 1] + 15);
  return 0;
}

int main(int argc,char **argv)
{
  _timer_clear(0);
  _timer_clear(1);
  _timer_start(0);

  if (parse_parameters(argc, argv) != 0)
  {
    printf("sample usage: compressor intra_compressed_trace_binary_* --read-thread=1 --merge-thread=1\n");
    return -1;
  }

  // adapt to old code without last two parameters
  argc -= 2;

  np=argc-1;
  proc_list=(uint32_t*)malloc(np*sizeof(int));

  // init
  argvs = argv;
  max_queue_size = argc  + 1;
  root_queue = (node_st2 **)malloc(max_queue_size * sizeof(node_st2 *));
  head = tail = 0;
  building_thread_count = 0;
  merging_thread_count = 0;
  
  pthread_t *threads = (pthread_t *)malloc(2 * argc * sizeof(pthread_t));
  int tc = 0;
  int *args = (int *)malloc(argc * sizeof(int));
  node_st2 **roots = (node_st2 **)malloc(2 * argc * sizeof(node_st2 *));

  if (pthread_mutex_init(&qmutex, NULL) != 0)
  {
    printf("error: can't init mutex.\n");
    return -1;
  }

  if (create_tree_buiding_threads(args, argc, threads, tc) != 0)
    return -1;
  if (create_tree_merging_threads(roots, threads, tc) != 0)
    return -1;
  
  // wait for all threads
  int n;
  for (n = 0; n < tc; ++n)
    pthread_join(threads[n], NULL);
  
  free(args);
  free(roots);
  pthread_mutex_destroy(&qmutex);

  _timer_start(1);

  node_st2 *root_st = root_queue[head];
  fp=fopen("compressed_trace_binary","w");
  final_write_binary(root_st,root_st->nchild+1);
  fclose(fp);
  fp=fopen("compressed_trace_text","w");
  final_write_text(root_st->next,root_st->nchild,0);
  fclose(fp);

  _timer_stop(0);
  _timer_stop(1);
  printf("whole time: %lf\nwrite time: %lf", _timer_read(0), _timer_read(1));

  return 0;
}
