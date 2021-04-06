#include "core.h"
#include "util.h"

int MPIPass::inFocus(string name){
#ifdef LLVM_FORTRAN
	transform(name.begin(), name.end(), name.begin(), ::tolower);
#endif
	if (focusFuncs.find(name) == focusFuncs.end())
		return 0;
	return focusFuncs[name];
}

bool MPIPass::isIgnored(Function *F){
	string name = F->getName();
	return ignore.find(name) != ignore.end();
}


void MPIPass::getCFAInfo(Function *F){
	func_t fi;
	CFAPass &cfa = getAnalysis<CFAPass>(*F);
	fi.nodes = cfa.nodes;
	fi.npos = vector<Record*>(fi.nodes.size(), NULL);
	funcMap[F] = fi;
}

void MPIPass::initialize(Module &M){
	//从in.txt中读取数据
	ifstream fin("in.txt");
	string func;
	focusFuncs.clear();
	int fid, ignoreCnt;
	fin>>ignoreCnt;
	while(ignoreCnt--)
	{
		fin>>func;
		ignore.insert(func);
	}

	while(fin>>fid>>func)
	{
#ifdef LLVM_FORTRAN
		transform(func.begin(), func.end(), func.begin(), ::tolower);
		func+="_";
#endif
		focusFuncs[func] = fid;
	}
	for(Module::iterator ftr = M.begin(); ftr != M.end(); ftr++)
		if(!ftr->empty() && !isIgnored(ftr))
			getCFAInfo(ftr);	
}


void MPIPass::FillOutSets(Function *F){

	for (int i = 0; i < funcMap[F].size(); i++)
	{
		BasicBlock *bp = funcMap[F].nodes[i].block;
		for (pred_iterator pb = pred_begin(bp); pb != pred_end(bp); pb++)
		{
			vector<ast_t> diff(funcMap[F].size());
			vector<ast_t>::iterator de;
			Node *pn = funcMap[F].getNode(*pb);
			de = set_difference(pn->aset.begin(), pn->aset.end(),
					funcMap[F].nodes[i].aset.begin(), funcMap[F].nodes[i].aset.end(),
					diff.begin());
			for (vector<ast_t>::iterator ai = diff.begin(); ai != de; ai++){
				Record *ir = funcMap[F].arMap[*ai];
				if (!ir){
					errs()<<"record not found\n";
					errs()<<F->getName()<<" "<<(*ai).id<<" "<<(*ai).type<<"\n";
					errs()<<"prev block"<<*(*pb);
				}
				assert(ir);
				ir->outSet.insert(bp);
			}
		}
	}
}

set<Record *> visted;
void MPIPass::CheckOutlets(Record *r){
	//if (r->outSet.size() == 2){}
	if (visted.find(r) != visted.end())
		return;
	visted.insert(r);
	for (int i = 0; i < r->child.size(); i++)
		CheckOutlets(r->child[i]);
}

bool MPIPass::CompressRecord(Record *r){
	for (vector<Record*>::iterator i = r->child.begin(); i != r->child.end(); )
	{
		bool del = CompressRecord(*i);
		if (del){
			Record *temp = *i;
			i = r->child.erase(i);
			delete temp;
		}
		else
			i++;
	}
	return r->empty();
}


void MPIPass::SetBranchType(Record *r){
	set<int> brset;
	for (int i = 0; i < (int)r->child.size(); i++)
	{
		int brid = r->child[i]->ast.brid;
		if (brset.find(brid) == brset.end()){
			r->child[i]->ast.brt = IF_BRANCH;
			brset.insert(brid);
		}
		else{
			r->child[i]->ast.brt =ELSE_BRANCH;
			brset.erase(brid);
		}
		SetBranchType(r->child[i]);
	}
}

Record *MPIPass::LocateRecord(BasicBlock *B){
	Function *F = B->getParent();
	Node *n = funcMap[F].getNode(B);
	int ind = n->id, giver = n->giver;

	if (ind == 0){
		funcMap[F].npos[ind] = funcMap[F].root;
		return funcMap[F].npos[ind];
	}
	if (!funcMap[F].npos[giver]){
		errs()<<B->getParent()->getName();
		errs()<<ind<<"'s giver is "<<giver;
		errs()<<*B<<*funcMap[F].nodes[giver].block;
	}
	assert(funcMap[F].npos[giver] && "giver should already Located");
	Record *gR = funcMap[F].npos[giver];
	if (n->own.id == 0){
		funcMap[F].npos[ind] = gR;
		return funcMap[F].npos[ind];
	}
	Record *nR = new Record();
	nR->inBB = B;
	nR->ast = n->own;
	funcMap[F].npos[ind] = nR;
	funcMap[F].arMap[n->own] = nR;
	gR->AddChild(nR);
	return funcMap[F].npos[ind];
}

void MPIPass::traverse(Function *F){
	funcMap[F].visted = true;
	for (Function::iterator bi = F->begin(); bi != F->end(); bi++)
	{
		Record *pos = LocateRecord(bi);
		assert(pos);
		for (BasicBlock::iterator ip = bi->begin(); ip != bi->end(); ip++)
		{
			if (ip->getOpcode() != Instruction::Call) continue;
			Function *callee = ((CallInst *)((Instruction *) ip))->getCalledFunction();
			string funcName;
			if (!callee)
			{
				Value *v;
#if LLVM_FORTRAN
				v = ip->getOperand(ip->getNumOperands()-1);
				v = ((ConstantExpr *)v)->getOperand(0);
				assert(v && isa<Function>(v));
#else
				v = ip->getOperand(0);
#endif
				funcName = ((Function *)v)->getName();
			}
			else
				funcName = callee->getName();
			int index = inFocus(funcName);
			//MPI_Sendrecv special handle
			if (index == 16){
				pos->AddChild(new Record(10));
				pos->AddChild(new Record(8));
				pos->AddChild(new Record(26));
				continue;
			}

			if (index){
				pos->AddChild(new Record(index));
				continue;
			}
			callee = (F->getParent())->getFunction(funcName);
			if (callee && !callee->empty() && !isIgnored(callee)){
				if (!funcMap[callee].visted)
					traverse(callee);
				if (!funcMap[callee].empty())
					pos->AddChild(funcMap[callee].root);
			}
		}
	}
	FillOutSets(F);
	//CheckOutlets(funcMap[F].root);
	CompressRecord(funcMap[F].root);
	SetBranchType(funcMap[F].root);
}

bool MPIPass::runOnModule(Module &M){
	initialize(M);
	Function *entry = M.getFunction("main");
	traverse(entry);
	AssignRecordID(funcMap[entry].root);
	InsertHints(funcMap[entry].root, M);
	DumpRecord(funcMap[entry].root);
	//PrintRecord(funcMap[entry].root, 4);
	return true;
}

void MPIPass::getAnalysisUsage(AnalysisUsage &AU) const{
	AU.addRequired<CFAPass>();

	//为什么CFAPass还是会调用好多次？
	AU.addPreserved<CFAPass>();
	AU.setPreservesAll();
}

char MPIPass::ID = 1;
int Record::cnt= 1;
static RegisterPass<MPIPass> X("mpi", "mpi analysis", false, false);
