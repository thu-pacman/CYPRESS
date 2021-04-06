//clw.201210.
//yfk.201502.
#include <stdlib.h>
#include <stdio.h>

#include "intracomp.h"

static node_st *first_st;
static else_stack *now_else,*else_mask;
static FILE *fp;

static int loop_trace_write_text(loop_trace *lt)
{
  if(lt==NULL)return 0;
  if(lt->dist==-1)
  {
    repeat_trace *rt;
    rt=(repeat_trace*)lt;
    loop_trace_write_text(rt->pre);
    fprintf(fp,"%d*[",rt->times);
    loop_trace_write_text((loop_trace *)(rt->trace));
    fprintf(fp,"]; ");
  }
  else
  {
    loop_trace_write_text(lt->pre);
    fprintf(fp,"%d*%d; ",lt->retimes,lt->times);
  }
  return 0;
}

static int branch_trace_write_text(branch_trace *lt)
{
  if(lt==NULL)return 0;
  if(lt->dist==-1)
  {
    repeat_trace *rt;
    rt=(repeat_trace*)lt;
    branch_trace_write_text(rt->pre);
    fprintf(fp,"%d*[",rt->times);
    branch_trace_write_text((branch_trace *)(rt->trace));
    fprintf(fp,"]; ");
  }
  else
  {
    branch_trace_write_text(lt->pre);
    char b;
    if(lt->torf==1)b='T';
    else b='F';
    fprintf(fp,"%d*%c; ",lt->retimes,b);
  }
  return 0;
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

static int p2p_trace_write_text(p2p_trace *lt)
{
  if(lt==NULL)return 0;
  if(lt->dist==-1)
  {
    repeat_trace *rt;
    rt=(repeat_trace*)lt;
    p2p_trace_write_text(rt->pre);
    fprintf(fp,"%d*[",rt->times);
    p2p_trace_write_text((p2p_trace *)(rt->trace));
    fprintf(fp,"]; ");
  }
  else
  {
    p2p_trace_write_text(lt->pre);
    fprintf(fp,"comm=%d,src=%d,tag=%d,dest=%d,times=%d,bytes=(",lt->comm_id,lt->src,lt->tag,lt->dest,lt->times);
  	bytes_write_text(lt->bytes);
  	fprintf(fp,"); ");
  }
  return 0;
}

static int coll_trace_write_text(coll_trace *lt)
{
  if(lt==NULL)return 0;
  if(lt->dist==-1)
  {
    repeat_trace *rt;
    rt=(repeat_trace*)lt;
    coll_trace_write_text(rt->pre);
    fprintf(fp,"%d*[",rt->times);
    coll_trace_write_text((coll_trace *)(rt->trace));
    fprintf(fp,"]; ");
  }
  else
  {
    coll_trace_write_text(lt->pre);
    fprintf(fp,"comm=%d,root=%d,sendbytes=%d,recvbytes=%d; ",lt->comm_id,lt->root_id,lt->send_bytes,lt->recv_bytes);
  }
  return 0;
}

static node_st *trace_write_text(node_st *nst,int nt,int nb)
{
  int n;
  while(nt>0)
  {
    if(nst==NULL)
    {
      printf("error: text write null node.\n");
      return NULL;
    }
    for(n=0;n<nb;n++)fprintf(fp,"| ");
    fprintf(fp,"|-");
    if((nst->tag==0)||(nst->tag==1))
    {
      fprintf(fp,"loop id=%d, total times=%d\n",nst->id,nst->times);
      for(n=0;n<nb+1;n++)fprintf(fp,"| ");
      loop_trace_write_text(nst->trace);
      fprintf(fp,"\n");
      if(nst!=trace_write_text(nst->next,nst->nchild,nb+1))printf("error: text loop write error.\n");
      nst=nst->other;
    }
    else if(nst->tag==2)
    {
      node_st *tst;
      fprintf(fp,"branch if id=%d, else id=%d, total times=%d\n",nst->id,nst->other->id,nst->times);
      for(n=0;n<nb+1;n++)fprintf(fp,"| ");
      branch_trace_write_text(nst->trace);
      fprintf(fp,"\n");
      tst=trace_write_text(nst->next,nst->nchild,nb+1);
      for(n=0;n<nb;n++)fprintf(fp,"| ");
      fprintf(fp,"|-");
      fprintf(fp,"else id=%d\n",nst->other->id);
      if(tst!=trace_write_text(nst->other->next,nst->other->nchild,nb+1))printf("error: text branch write error.\n");
      nst=tst;
      nt--;
      continue;
    }
    else if(nst->tag==5)
    {
      fprintf(fp,"p2p id=%d, total times=%d, sum_time=%.0f, sum_sqr_time=%.0f\n",nst->id,nst->times,nst->sum_time,nst->sum_sqr_time);
      for(n=0;n<nb+1;n++)fprintf(fp,"| ");
      p2p_trace_write_text(nst->trace);
      fprintf(fp,"\n");
    }
    else if(nst->tag==6)
    {
      fprintf(fp,"coll id=%d, total times=%d, sum_time=%.0f, sum_sqr_time=%.0f\n",nst->id,nst->times,nst->sum_time,nst->sum_sqr_time);
      for(n=0;n<nb+1;n++)fprintf(fp,"| ");
      coll_trace_write_text(nst->trace);
      fprintf(fp,"\n");
    }
    else if(nst->tag==7)
    {
      fprintf(fp,"wait id=%d, total times=%d, sum_time=%.0f, sum_sqr_time=%.0f\n",nst->id,nst->times,nst->sum_time,nst->sum_sqr_time);
      for(n=0;n<nb+1;n++)fprintf(fp,"| ");
      //TODO
      fprintf(fp,"\n");
    }
    else if(nst->tag==8)
    {
      fprintf(fp,"waitall id=%d, total times=%d, sum_time=%.0f, sum_sqr_time=%.0f\n",nst->id,nst->times,nst->sum_time,nst->sum_sqr_time);
      for(n=0;n<nb+1;n++)fprintf(fp,"| ");
      //TODO
      fprintf(fp,"\n");
    }
    else if(nst->tag==9)
    {
      fprintf(fp,"comm id=%d, total times=%d\n",nst->id,nst->times);
      for(n=0;n<nb+1;n++)fprintf(fp,"| ");
      //TODO
      fprintf(fp,"\n");
    }
    nst=nst->next;
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

static int loop_trace_write_binary(loop_trace *lt)
{
  int16_t n;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      repeat_trace *rt;
      rt=(repeat_trace*)lt;
      repeat_trace_write_binary(rt);
      loop_trace_write_binary((loop_trace *)(rt->trace));
      lt=(loop_trace*)rt->pre;
    }
    else
    {
      fwrite(&lt->dist,2,1,fp);
      fwrite(&lt->times,4,1,fp);
      fwrite(&lt->retimes,4,1,fp);
      lt=lt->pre;
    }
  }
  n=-2;
  fwrite(&n,2,1,fp);
  return 0;
}

static int branch_trace_write_binary(branch_trace *lt)
{
  int16_t n;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      repeat_trace *rt;
      rt=(repeat_trace*)lt;
      repeat_trace_write_binary(rt);
      branch_trace_write_binary((branch_trace *)(rt->trace));
      lt=(branch_trace*)rt->pre;
    }
    else{
      fwrite(&lt->dist,2,1,fp);
      fwrite(&lt->torf,2,1,fp);
      fwrite(&lt->retimes,4,1,fp);
      lt=lt->pre;
    }
  }
  n=-2;
  fwrite(&n,2,1,fp);
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

static int p2p_trace_write_binary(p2p_trace *lt)
{
  int16_t n;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      repeat_trace *rt;
      rt=(repeat_trace*)lt;
      repeat_trace_write_binary(rt);
      p2p_trace_write_binary((p2p_trace *)(rt->trace));
      lt=(p2p_trace*)rt->pre;
    }
    else{
      fwrite(&lt->dist,2,1,fp);
      fwrite(&lt->comm_id,2,1,fp);
      fwrite(&lt->src,2,1,fp);
      fwrite(&lt->tag,2,1,fp);
      fwrite(&lt->dest,2,1,fp);
      fwrite(&lt->times,4,1,fp);
      bytes_write_binary(lt->bytes);
      lt=lt->pre;
    }
  }
  n=-2;
  fwrite(&n,2,1,fp);
  return 0;
}

static int coll_trace_write_binary(coll_trace *lt)
{
  int16_t n;
  while(lt!=NULL)
  {
    if(lt->dist==-1)
    {
      repeat_trace *rt;
      rt=(repeat_trace*)lt;
      repeat_trace_write_binary(rt);
      coll_trace_write_binary((coll_trace *)(rt->trace));
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
  n=-2;
  fwrite(&n,2,1,fp);
  return 0;
}

static node_st *trace_write_binary(node_st *nst,int nt)
{
  for(;nt>0;nt--)
  {
    if(nst==NULL)
    {
      printf("error: binary write null node.\n");
      return NULL;
    }
    if((nst->tag==0)||(nst->tag==1))
    {
      nst->tag=1;
      fwrite(&nst->tag,2,1,fp);
      fwrite(&nst->id,2,1,fp);
      fwrite(&nst->times,4,1,fp);
      fwrite(&nst->nchild,4,1,fp);
      loop_trace_write_binary(nst->trace);
      if(nst!=trace_write_binary(nst->next,nst->nchild))printf("error: binary loop write error.\n");
      nst=nst->other;
    }
    else if(nst->tag==2)
    {
      node_st *tst;
      fwrite(&nst->tag,2,1,fp);
      fwrite(&nst->id,2,1,fp);
      fwrite(&nst->times,4,1,fp);
      fwrite(&nst->nchild,4,1,fp);
      fwrite(&nst->other->id,2,1,fp);
      fwrite(&nst->other->nchild,4,1,fp);
      branch_trace_write_binary(nst->trace);
      tst=trace_write_binary(nst->next,nst->nchild);
      if(tst!=trace_write_binary(nst->other->next,nst->other->nchild))printf("error: binary branch write error.\n");
      nst=tst;
      continue;
    }
    else if(nst->tag==5)
    {
      fwrite(&nst->tag,2,1,fp);
      fwrite(&nst->id,2,1,fp);
      fwrite(&nst->times,4,1,fp);
      fwrite(&nst->sum_time,sizeof(double),1,fp);
      fwrite(&nst->sum_sqr_time,sizeof(double),1,fp);
      p2p_trace_write_binary(nst->trace);
    }
    else if(nst->tag==6)
    {
      fwrite(&nst->tag,2,1,fp);
      fwrite(&nst->id,2,1,fp);
      fwrite(&nst->times,4,1,fp);
      fwrite(&nst->sum_time,sizeof(double),1,fp);
      fwrite(&nst->sum_sqr_time,sizeof(double),1,fp);
      coll_trace_write_binary(nst->trace);
    }
    else if(nst->tag==7)
    {
      fwrite(&nst->tag,2,1,fp);
      fwrite(&nst->id,2,1,fp);
      fwrite(&nst->times,4,1,fp);
      fwrite(&nst->sum_time,sizeof(double),1,fp);
      fwrite(&nst->sum_sqr_time,sizeof(double),1,fp);
      //TODO
    }
    else if(nst->tag==8)
    {
      fwrite(&nst->tag,2,1,fp);
      fwrite(&nst->id,2,1,fp);
      fwrite(&nst->times,4,1,fp);
      fwrite(&nst->sum_time,sizeof(double),1,fp);
      fwrite(&nst->sum_sqr_time,sizeof(double),1,fp);
      //TODO
    }
    else if(nst->tag==9)
    {
      fwrite(&nst->tag,2,1,fp);
      fwrite(&nst->id,2,1,fp);
      fwrite(&nst->times,4,1,fp);
      //TODO
    }
    else if(nst->tag==13)
    {
      fwrite(&nst->times,4,1,fp);
      fwrite(&nst->nchild,4,1,fp);
    }
    else printf("error: unknow tag.\n");
    nst=nst->next;
  }
  return nst;
}

static int struct_read(node_st **pnst,int nc)
{
  int n,nt,fun,loop,branch,id,nchild;
  node_st *nst,*tst;
  else_stack *tel;
  nst=*pnst;
  nt=0;
  for(n=0;n<nc;n++)
  {
    fscanf(fp,"%d %d %d %d %d",&fun,&loop,&branch,&id,&nchild);
    if(fun>0)
    {
      nst->next=(node_st*)malloc(sizeof(node_st));
      nst=nst->next;
      while((now_else->tag>0)&&(now_else!=else_mask))
      {
        now_else->elst->next=nst;
        tel=now_else;
        now_else=now_else->pre;
        free(tel);
      }
      //funcid2tag
      if(fun<16)nst->tag=5;
      else if((fun>34)&&(fun<47))nst->tag=6;
      else if((fun==25)||(fun==27)||(fun==31))nst->tag=7;
      else if((fun==26)||(fun==32))nst->tag=8;
      else if(fun>49)nst->tag=9;
      else printf("error: read wrong function id.\n");
      nst->id=fun;
      nst->times=0;
      nst->sum_time=0;
      nst->sum_sqr_time=0;
      nst->trace=NULL;
      nst->next=NULL;
      nst->other=NULL;
      nt++;
    }
    else if(loop==1)
    {
      nst->next=(node_st*)malloc(sizeof(node_st));
      nst=nst->next;
      while((now_else->tag>0)&&(now_else!=else_mask))
      {
        now_else->elst->next=nst;
        tel=now_else;
        now_else=now_else->pre;
        free(tel);
      }
      nst->tag=0;
      nst->id=id;
      nst->times=0;
      nst->trace=NULL;
      nst->next=NULL;
      tst=nst;
      nst->nchild=struct_read(&tst,nchild);
      tst->next=nst;
      while((now_else->tag>0)&&(now_else!=else_mask))
      {
        now_else->elst->next=nst;
        tel=now_else;
        now_else=now_else->pre;
        free(tel);
      }
      nst->other=(node_st*)malloc(sizeof(node_st));
      nst=nst->other;
      nst->next=NULL;
      nst->other=NULL;
      nt++;
    }
    else if((fun==-2)&&(branch==1))
    {
      nst->next=(node_st*)malloc(sizeof(node_st));
      nst=nst->next;
      while((now_else->tag>0)&&(now_else!=else_mask))
      {
        now_else->elst->next=nst;
        tel=now_else;
        now_else=now_else->pre;
        free(tel);
      }
      nst->tag=2;
      nst->id=id;
      nst->times=0;
      nst->trace=NULL;
      nst->next=NULL;
      nst->other=(node_st*)malloc(sizeof(node_st));
      tst=nst->other;
      nst->nchild=struct_read(&nst,nchild);
      tst->id=0;
      tst->nchild=0;
      tst->trace=NULL;
      tst->next=NULL;
      tst->other=NULL;
      tel=(else_stack*)malloc(sizeof(else_stack));
      tel->tag=1;
      tel->elst=tst;
      tel->pre=now_else;
      now_else=tel;
      nt++;
    }
    else if((fun==-3)&&(branch==1))
    {
      if(now_else->tag!=1)printf("error: read else node not found.\n");
      tst=now_else->elst;
      tst->id=id;
      tel=else_mask;
      else_mask=now_else;
      tst->nchild=struct_read(&now_else->elst,nchild);
      else_mask=tel;
      now_else->tag=2;
    }
    else{
      nt+=struct_read(&nst,nchild);
    }
  }
  *pnst=nst;
  return nt;
}

extern node_st *compress_init(const int pid)
{
  int fun,loop,branch,id,nchild;
  node_st *temp;
  nchild=0;
  fp=fopen("out.txt","r");
  if(fp==NULL)printf("error: struct file not found.\n");
  else fscanf(fp,"%d %d %d %d %d",&fun,&loop,&branch,&id,&nchild);
  first_st=(node_st*)malloc(sizeof(node_st));
  first_st->tag=13;	//root
  first_st->times=pid;
  first_st->trace=NULL;
  first_st->next=NULL;
  first_st->other=NULL;
  now_else=(else_stack*)malloc(sizeof(else_stack));
  now_else->tag=0;
  now_else->elst=NULL;
  now_else->pre=NULL;
  else_mask=NULL;
  temp=first_st;
  first_st->nchild=struct_read(&temp,nchild);
  fclose(fp);
  return first_st;
}

extern int compress_fini()
{
  char fn[256];
  sprintf(fn,"intra_compressed_trace_binary_%04d",first_st->times);
  fp=fopen(fn,"w");
  trace_write_binary(first_st,first_st->nchild+1);
  fclose(fp);
  sprintf(fn,"intra_compressed_trace_text_%04d",first_st->times);
  fp=fopen(fn,"w");
  trace_write_text(first_st->next,first_st->nchild,0);
  fclose(fp);
  return 0;
}

