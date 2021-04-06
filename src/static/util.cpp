#include "util.h"


static void print_blanks(int cnt){
	while(cnt--)
		errs()<<" ";
}

void PrintRecord(Record *r, int blankCnt){
	print_blanks(blankCnt);
	errs()<<"//";
	if (r->ast.type == MPI_LOOP)
		errs()<<"loop ";
	if (r->ast.type == MPI_BRANCH)
		errs()<<"branch ";
	errs()<<"\n";
	for (int i = 0; i < r->child.size(); i++)
	{
		if (r->child[i]->fun > 0){
			print_blanks(blankCnt);
			errs()<<r->child[i]->fun<<"\n";
		}
		else
			PrintRecord(r->child[i], blankCnt+4);
	}
}

void AssignRecordID(Record *r){
	if (r->rid != -1)
		return;
	r->rid = Record::cnt++;
//	if (r->rid == 31)
//		errs()<<"31's block "<<r->inBB->getParent()->getName()<<*r->inBB;
	for (int i = 0; i < r->child.size(); i++)
		AssignRecordID(r->child[i]);
}

void DumpRecordNode(Record *r, ofstream &out){
	int inloop = r->ast.type == MPI_LOOP? 1 : 0;
	int inbranch = r->ast.type == MPI_BRANCH? 1 : 0;
	int fun = r->fun;
	if (inbranch)
		fun = r->ast.brt;
	out<<fun<<" "<<inloop<<" "<<inbranch<<" "<<r->rid<<" "<<r->child.size()\
		<<endl;
	for (int i = 0; i < r->child.size(); i++)
		DumpRecordNode(r->child[i], out);
}

void DumpRecord(Record *r){
	ofstream out("out.txt");
	DumpRecordNode(r, out);
	out.close();
}

static Function *mpi_pattern, *mpi_pattern_exit;
static set<Record *> recInsMap;

void PrepareFuncs(Module &M){
	LLVMContext &context = M.getContext();
	vector<Type *> func_args;
	func_args.push_back(IntegerType::get(context, 32));
	func_args.push_back(IntegerType::get(context, 32));
	FunctionType *funType = FunctionType::get(Type::getVoidTy(context),
			func_args,false);
	mpi_pattern = Function::Create(funType, GlobalValue::ExternalLinkage, 
			"mpi_pattern", &M);
	mpi_pattern_exit = Function::Create(funType, GlobalValue::ExternalLinkage, 
			"mpi_pattern_exit", &M);
}


Instruction *getPatternPos(Record *r){
	BasicBlock::iterator pos = r->inBB->begin();
	for ( ; pos != r->inBB->end(); pos++){
		if (pos->getOpcode() == Instruction::PHI)
			continue;
		if (pos->getOpcode() == Instruction::Call){
			Function *callee = ((CallInst *)(Instruction *)pos)->getCalledFunction();
			if (callee && callee->getName() == "mpi_pattern_exit"){
				errs()<<"mpi_patter_exit inserted found\n";
				continue;
			}
		}
		break;
	}
	assert( (Instruction *)pos);
	return pos;
}

void InsertAtRecord(Record *r, Module &M){
	if (r->fun > 0 || recInsMap.find(r) != recInsMap.end())
		return;
	recInsMap.insert(r);
	vector<Value *> args;
	char buf[50];
	int type = r->ast.type;
	sprintf(buf, "%d", type);
	args.push_back(ConstantInt::get(M.getContext(), APInt(32, StringRef(buf), 10)));
	sprintf(buf, "%d", r->rid);
	args.push_back(ConstantInt::get(M.getContext(), APInt(32, StringRef(buf), 10)));
	Instruction *insPos;

	if (r->ast.type != 0 && (type == MPI_LOOP || r->ast.brt == IF_BRANCH))
	{
		for (set<BasicBlock*>::iterator bs = r->outSet.begin(); 
				bs != r->outSet.end(); bs++)
		{
			insPos = (*bs)->getFirstNonPHI();
			assert(insPos && "all instructions are PHINode!");
			CallInst::Create(mpi_pattern_exit, args, "", insPos);
		}
	}

	for (int i = 0; i < r->child.size(); i++)
		InsertAtRecord(r->child[i], M);
	if (r->ast.type != 0){
		insPos = getPatternPos(r);
		assert(insPos);
		CallInst::Create(mpi_pattern, args, "", insPos);
	}
}


void InsertHints(Record *root, Module &M){
	PrepareFuncs(M);
	recInsMap = set<Record *>();
	InsertAtRecord(root, M);
}

