#ifndef CORE_H
#define CORE_H

#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/BasicBlock.h"
#include "llvm/Instructions.h"
#include "llvm/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/CallGraphSCCPass.h"
#include <map>
#include <set>
#include <stack>
#include <string>
#include <iostream>
#include <algorithm>
#include <stdlib.h>
#include <fstream>
#include <cstdio>
#include <cctype>
#include "cfa.h"

using namespace llvm;
using namespace std;



class Record
{
	public:
		static int cnt;
		vector<Record *> child;
		int rid;
		int fun;
		ast_t ast;

		BasicBlock *inBB;
		set<BasicBlock *> outSet;

		Record(int index = -1, bool leaf = false)
		{
			fun = index;
			rid = -1;
			inBB = NULL;
			outSet = set<BasicBlock*>();
		}

		void AddChild(Record *r){
			child.push_back(r);
		}

		bool empty(void){
			if (fun > 0)
				return false;
			for (int i = 0; i < child.size(); i++)
				if (!child[i]->empty())
					return false;
			return true;
		}

		void AddOutlets(BasicBlock *B, PostDominatorTree &);
};


class func_t
{
	public:
		vector<Node> nodes;
		map<set<ast_t>, Record*> posMap;
		map<ast_t, Record*> arMap;

		//giver branch
		//node pos, npos[nodes[i].giver]即为i's giver插入的Record位置
		vector<Record *> npos;

		Record *root;
		bool visted;


		size_t size(){
			return nodes.size();
		}

		func_t()
		{
			root = new Record();
			posMap = map<set<ast_t>, Record*>();
			arMap = map<ast_t, Record *>();
			visted = false;
		}

		bool empty(){
			return root->empty();
		}

		Node *getNode(BasicBlock *BB){
			for (int i = 0; i < nodes.size(); i++)
				if (nodes[i].block == BB)
					return &nodes[i];
			assert(0 && "getNode(BasicBlock *) fails.");
			return NULL;
		}
};


class MPIPass: public ModulePass
{
	public:
		static char ID;
		MPIPass() : ModulePass(ID) {}

		virtual void getAnalysisUsage(AnalysisUsage &AU) const;
		virtual bool runOnModule(Module &M);

		map<string,int> focusFuncs;
		set<string> ignore;//包含goto的函数，不处理

		map<Function *, func_t> funcMap;

		//从函数F中读取信息，得到func_t，塞进funcMap
		void getCFAInfo(Function *F);

		void initialize(Module &);

		void traverse(Function *);

		//函数funcs是否在输入文件的函数列表中,若是mpi函数，返回序号,反之返回0
		int inFocus(string func);

		//函数func是否不分析的函数列表中(包含goto)
		bool isIgnored(Function *F);

		Record *LocateRecord(BasicBlock *);

		//对函数F对应的Record树,计算每个Record的出口集合
		void FillOutSets(Function *F);

		void CheckOutlets(Record *);

		//将没有叶子的分支砍掉
		bool CompressRecord(Record *r);

		//设置if/else branch的类型 
		void SetBranchType(Record *r);

		//修复那些没有匹配if的else Record节点
		int TransformBranch(Record *r){}

};

#endif

