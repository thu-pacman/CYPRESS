#include "cfa.h"
#include <queue>

void CFAPass::initialize(Function &F)
{
	ast_t::cnt = 1;

	//Attention: 需要清空vector!
	nodes.clear();
	edges.clear();
	parentRel.clear();
	
	//初始化节点
	int i = 0, n = F.size();
	for (Function::iterator bp = F.begin(); bp != F.end(); bp++)
		nodes.push_back(Node(bp, i++));

	//初始化边
	edges = vector<vector<bool> >(n, vector<bool>(n, false));
	for (int i = 0; i < n; i++){
		BasicBlock *bp = nodes[i].block;
		for (succ_iterator s = succ_begin(bp); s != succ_end(bp); s++)
			edges[i][indexOf(*s)] = true;
	}

	//初始化parentRel
	parentRel.resize(n);
	for (int i = 0; i < n; i++){
		parentRel[i].resize(n);
		for (int j = 0; j < n; j++)
			parentRel[i][j] = false;
	}
}

void PrintNode(Node n){
	errs()<<"node "<<n.id+1<<"\n";
	for (set<ast_t>::iterator itr = n.aset.begin();
			itr != n.aset.end(); itr++){
		if(itr->type == MPI_LOOP)
			errs()<<"loop";
		else 
			errs()<<"branch";
		errs()<<itr->id;
		if (itr->id == n.own.id)
			errs()<<"*";
		errs()<<" ";
	}
	errs()<<"\n";
}

void CFAPass::Print(void){
	errs()<<nodes[0].block->getParent()->getName()<<"\n";
	for (int i = 0; i < nodes.size(); i++)
		PrintNode(nodes[i]);
}

void CFAPass::SetParent(int child, int parent){
	nodes[child].parent = parent;
	for ( int i = 0; i < nodes.size(); i++)
		parentRel[i][child] = parentRel[i][parent];
	parentRel[parent][child] = true;
}

//TODO： nodes[ind].own = ast(0, 0);
int CFAPass::RemoveItsOwnBranch(int ind){
	int found = 0, parent = nodes[ind].parent;
	ast_t rev;

	vector<ast_t> diff(nodes.size());
	vector<ast_t>::iterator de;
	de= set_difference(nodes[ind].aset.begin(), nodes[ind].aset.end(),
			nodes[parent].aset.begin(), nodes[parent].aset.end(),
			diff.begin());

	for (vector<ast_t>::iterator ri = diff.begin(); ri != diff.end(); ri++){
		if (ri->type == MPI_BRANCH){
			found++;
			rev = *ri;
		}
	}

	for (int i = 0; i < nodes.size(); i++)
		if (nodes[i].aset.find(rev) != nodes[i].aset.end())
			nodes[i].aset.erase(rev);
	return found;
}


void CFAPass::MarkAsLoop(int ind, int head, ast_t loop){
	vector<bool> marked(nodes.size(), false);
	marked[ind] = true;
	nodes[ind].aset.insert(loop);
	queue<int> que;
	que.push(ind);

	while(!que.empty())
	{
		int n = que.front();
		que.pop();
		if (n == head) continue;
		for (int i = 0; i < nodes.size(); i++)
		{
			if (!edges[i][n] || marked[i]) continue;
			marked[i] = true;
			nodes[i].aset.insert(loop);
			que.push(i);
		}
	}
}


//FORTRAN里面也有和C循环结构相同的while循环,所以也进行一次C的RemoveItsOwnBranch
//但是考虑可能有break的,所以暂不修改为一般性的找循环出口,遇到了再说
//循环出口不会标记为loop，所以无需处理
void CFAPass::SetLoop(int head, int tail){
	ast_t loop(MPI_LOOP, ast_t::cnt++);

	nodes[head].own = loop;
	MarkAsLoop(tail, head, loop);
	//Attention!
	//需要检查head是不是和非循环节点相连才RemoveItsOwnBranch(brrNode)
	int found = 0, brrNode = -1, parent = nodes[tail].parent;
	bool link = false;//标识节点是否和循环外的节点相连
	for (int i = 0; i < nodes.size(); i++)
	{
		if(!edges[head][i])	continue;
		if(nodes[i].aset.find(loop) != nodes[i].aset.end())
			brrNode = i;
		else 
			link = true;
	}
	if (link && brrNode != -1)
		found |= RemoveItsOwnBranch(brrNode);
	link = false;
	for (int i = 0; i < nodes.size(); i++)
	{
		if (!edges[parent][i]) continue;
		if (nodes[i].aset.find(loop) == nodes[i].aset.end())
			link = true;
	}
	if (link)
		found |= RemoveItsOwnBranch(tail);
#if 0
	if (!found){
		errs()<<"head..."<<*(nodes[head].block)<<"tail..."<<*nodes[tail].block;
		PrintNode(nodes[brrNode]);
		PrintNode(nodes[tail]);
	}
#endif
	//存在出口在其他地方的情况，所以可能found != 1
	//assert( found == 1 && "found should be 1(may while(1) break loop)");

	//Fortran custom: if/do/while
#ifdef LLVM_FORTRAN
	found = RemoveItsOwnBranch(head);
	assert( (found == 0 || found == 1) && "found should be 0 or 1");
#endif
}

bool CFAPass::isParent(int parent, int child){
	return parentRel[parent][child];
}

void CFAPass::InheritAset(int child, int giver){
	nodes[child].giver = giver;
	nodes[child].aset = set<ast_t>(nodes[giver].aset);
}

void CFAPass::AddBranch(int ind, int brid){
	ast_t br(MPI_BRANCH, ast_t::cnt++, brid);
	nodes[ind].own = br;
	nodes[ind].aset.insert(br);
}

void CFAPass::CalculateAset(int ind){
	PostDominatorTree &pdt = getAnalysis<PostDominatorTree>();
	int p, q;
	p = q = nodes[ind].parent;
	while(p != -1 && pdt.dominates(nodes[ind].block, nodes[p].block)){
		q = p;
		p = nodes[p].parent;
	}
	assert(q != -1 && "Error in CalculateAset()");
	InheritAset(ind, q);
}


void CFAPass::bfs(Function &F){
	PostDominatorTree &pdt = getAnalysis<PostDominatorTree>();
	queue<int> que;
	que.push(0);
	nodes[0].visited = true;

	while(!que.empty())
	{
		int n = que.front();
		que.pop();
		for (int i = 0; i < nodes.size(); i++)
		{
			if (!edges[n][i]) continue;
			if (!nodes[i].visited)
			{
				nodes[i].visited = true;
				SetParent(i, n);
				que.push(i);
				if (!pdt.dominates(nodes[i].block, nodes[n].block)){
					InheritAset(i, n);
					AddBranch(i, n);
				}
				else
					CalculateAset(i);
			}
			else if(isParent(i, n))
				SetLoop(i, n);
		}
	}
}

void CFAPass::Check(void){
	set<set<ast_t> > ss;
	set<ast_t> empty;
	empty.clear();
	ss.clear();
	ss.insert(empty);

	for (int i = 0; i < nodes.size(); i++)
	{
		if (ss.find(nodes[i].aset) != ss.end())
			continue;
		set<ast_t> one = nodes[i].aset;
		one.erase(nodes[i].own);
		if (ss.find(one) != ss.end())
			ss.insert(nodes[i].aset);
		else{
			PrintNode(nodes[i]);
			errs()<<*nodes[i].block<<"\nblock own ast:"<<nodes[i].own.id<<"giver info\n";
			int giver = nodes[i].giver;
			PrintNode(nodes[giver]);
			errs()<<*nodes[giver].block;
			assert(0 && "should found less one aset!\n");
		}
	}
	ss.clear();
}


bool CFAPass::runOnFunction(Function &F)
{
	string name = F.getName();
	initialize(F);
	bfs(F);
if (name == "__sprk_MOD_remove_droplets")
	Print();
	
	// Check的assertion不对，因为可以giver: ... loop1 node: branch ... branch2
	//	Check();
	return false;
}

int ast_t::cnt = 1;
char CFAPass::ID = 0;
static RegisterPass<CFAPass> X("cfa", "control flow analysis", true, true);
