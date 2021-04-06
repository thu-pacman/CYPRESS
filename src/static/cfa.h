#ifndef CFA_H
#define CFA_H

#include "llvm/Pass.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CFG.h"
#include "llvm/Analysis/PostDominators.h"
#include <vector>
#include <set>
#include <map>
#include <algorithm>

#define MPI_LOOP 1
#define MPI_BRANCH 2

#define IF_BRANCH -2
#define ELSE_BRANCH -3

using namespace llvm;
using namespace std;

class ast_t{
public:
	static int cnt; 
	int type;
	int id;
	int brid;
	int brt;//branch type

	ast_t(int t = 0, int i = 0, int bid = -1)
	{
		type = t;
		id = i;
		brid = bid;
		brt = -1;
	}

	bool operator<(const ast_t &m) const{
		return this->id < m.id;
	}
	bool operator!=(const ast_t &m) const{
		return this->id != m.id;
	}
	bool operator==(const ast_t &m) const{
		return cnt == m.cnt && type == m.type && id == m.id;
	}
};

class Node
{
	public:
		static int branchCnt;

		BasicBlock *block;
		int id;
		set<ast_t> aset;
		ast_t own;

		bool visited;
		int parent;
		//记录当前节点继承哪个节点的aset
		int giver;

		Node(BasicBlock *bb = NULL,int nid= -1){
			block = bb;
			id = nid;
			visited = false;
			parent = giver = -1;
			aset.clear();
		}
};

struct CFAPass: public FunctionPass 
{
	static char ID;
	vector<Node> nodes;

	//edges[i][j]== true: i->j 存在一条边
	vector<vector<bool> > edges;

	//parentRel[i][j] == true 表示i是j的祖先
	vector<vector<bool> > parentRel;

	CFAPass() : FunctionPass(ID) {}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const{
		AU.addRequired<PostDominatorTree>();
		AU.setPreservesAll();
	}

	virtual bool runOnFunction(Function &F);

	int indexOf(BasicBlock *BB){
		for (int i = 0; i < nodes.size(); i++)
			if (nodes[i].block == BB)
				return i;
		assert(0 && "indexOf() fail to find corresponding node");
		return -1;
	}

	void initialize(Function &F);

	void bfs(Function &F);

	//child的parent = parent的祖先 + parent
	void SetParent(int child, int parent);

	//去掉节点的branch ast，该节点位与分支的起始处
	//fortran if do while的时候head和head都有这个需求,但fortran不是所有的
	//loop都置于一层if do while， 也有while do 的结构,不能错误的假设
	int RemoveItsOwnBranch(int ind);

	//从ind反向搜索到的节点，标记为loop
	void  MarkAsLoop(int ind, int head, ast_t loop);

	//将head, tail之间的所有节点添加loop ast_t
	void SetLoop(int head, int tail);

	//parent节点是child节点的祖先
	bool isParent(int parent, int child);

	//将giver的ast集合拷贝到child中
	void InheritAset(int child, int giver);

	//将branch的ast_t添加到节点m的集合中
	void AddBranch(int node, int brType);

	//先找到m PostDominates的父节点n,再继承n的集合
	//可以考虑m PostDominate n && n Dominate m
	void CalculateAset(int node);

	//额外的检查，判断一个node相对与parent而言是不是只增加了一个ast
	void Check(void);

	//打印各节点的信息：每个节点 哪些ast in，哪些ast out
	void Print(void);

};

//打印Node n,放在外面是方便core调用用于调试
extern void PrintNode(Node n);

#endif
