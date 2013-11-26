
#include "CTaintAnalysis.h"
#include "intra-ctaint.h"
#include "inter-ctaint.h"

#include <llvm/Support/InstIterator.h>
#include <llvm/Target/Mangler.h>

#include <fstream>
#include <sstream>

char CTaintAnalysis::ID = 0;

const string CTaintAnalysis::_taintId("[STTAL]");
const string CTaintAnalysis::_taintSourceFile("cfg/sources.cfg");

const int CTaintAnalysis::_SOURCE_ARG_RET(-1);
const int CTaintAnalysis::_SOURCE_ARG_INVALID_MIN(-2);
const int CTaintAnalysis::_FUNCTION_NOT_SOURCE(100);

map<string, int> CTaintAnalysis::_taintSources;
typedef map<string, int>::value_type sourceType;

inline void CTaintAnalysis::log(const string &msg) {
	errs() << _taintId << msg << '\n';
}

void CTaintAnalysis::addTaintSource(string &source, unsigned taintedPos) {
	map<string, int>::iterator it;
	it = _taintSources.find(source);

	if (it != _taintSources.end())
		it->second = taintedPos;
	else
		_taintSources.insert( sourceType(source, taintedPos) );
}

void CTaintAnalysis::readTaintSourceConfig() {
	//Open the file with taint source functions listed
	std::ifstream srcFile(_taintSourceFile.c_str());
	string line;
	string source;
	string arg;
	int pos;

	while (!srcFile.eof()) {
		std::getline(srcFile, line);
		if (!line.empty()) {
			size_t coma = line.find(SOURCE_ARG_DELIM);
			source = line.substr(0, coma);

			arg = line.substr(coma+1);
			pos = atoi(arg.c_str());

			//TODO: use the mangler here Mangler aMangler
			string s("__isoc99_");
			s.append(source);
			addTaintSource(s, pos);
		}
	}

	srcFile.close();
}

CTaintAnalysis::CTaintAnalysis() : ModulePass(ID),
									_intraFlag(false),
									_interFlag(false),
									_interContextSensitiveFlag(false),
									_pointerMain(0),
									_super(0),
									_predInst(0){
	_super = static_cast<InstVisitor<CTaintAnalysis> *>(this);
	readTaintSourceConfig();
}

CTaintAnalysis::~CTaintAnalysis() {

	{//Delete _summaryTable info
		Function *F = 0;
		for(unsigned k = 0; k < _allProcsTPOrder.size(); ++k) {
			F = _allProcsTPOrder[k];
			vector<bool> * argVec = _summaryTable[F];
			delete argVec;
		}
	}

}

void CTaintAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.setPreservesAll();
	AU.addRequired<CallGraph> ();
	AU.addRequired<EQTDDataStructures> ();
}

void CTaintAnalysis::printOut(Instruction &I) {
	set<Value *> &out = _OUT[&I];
	errs() << "printOut for: "; I.print(errs()); errs() << "\n";
	for( set<Value *>::iterator it = out.begin(), itEnd = out.end();
			it != itEnd;
			++it) {
		(*it)->print(errs());
		errs() << "\n";
	}
	errs() << "printOut for: "; I.print(errs()); errs() << " END\n";
}

void CTaintAnalysis::printIn(Instruction &I) {
	set<Value *> &in = _IN[&I];
	for( set<Value *>::iterator it = in.begin(), itEnd = in.end();
			it != itEnd;
			++it) {
		(*it)->print(errs());
		errs() << "\n";
	}
}

bool CTaintAnalysis::runOnModule(Module &m) {
	log("module identifier is " + m.getModuleIdentifier());

	_aliasInfo = &getAnalysis<EQTDDataStructures>();

	_cg = &getAnalysis<CallGraph>();

	assert(_aliasInfo && "An instance of DSA could not be created!");

	assert(_cg && "An instance of the callgraph could not be created!");

	Function *f = 0;
	for(CallGraph::iterator pc = _cg->begin(), Epc = _cg->end();
			pc != Epc;
			++pc) {
		f = const_cast<Function *>(pc->first);

		//We only handle function defined in the code
		if (!f)
			continue;

		if(f->isDeclaration())
			continue;

		string fName(f->getName().str());
		log("discovered function " + fName);


		//TODO: use function signature as key instead
		_signatureToFunc[fName] = f;
		_allProcsTPOrder.push_back(f);

		{//Initialize _summaryTable info.
			vector<bool> *argVec = new vector<bool>;
			for(Function::arg_iterator A = f->arg_begin(), E=f->arg_end(); A != E; ++A) {
				argVec->push_back(false);
			}
			//Add an element if the function has a non-void type
			if (!f->getReturnType()->isVoidTy())
				argVec->push_back(false);

			_summaryTable[f] = argVec;
		}

		{//Initialize alias info from DSA
			if (_aliasInfo->hasDSGraph(*f))
				_functionToDSGraph[f] = _aliasInfo->getDSGraph(*f);
		}

		if ( !_pointerMain && 0 == fName.compare(ENTRY_POINT) ) {
			_pointerMain = f;
		}
	}

	//_aliasInfo->print(errs(), &m);

	//log("Now performs the intraprocedural analysis");
	//CTaintIntraProcedural intraFlow(this);
	CTaintInterProcedural interFlow(this);

	return false;
}


/**
 * Checks if the function with name 'funcName' taints any
 * of its parameters.
 */
int CTaintAnalysis::isTaintSource(string &funcName) {
	map<string, int>::iterator res;
	res = _taintSources.find(funcName);
	if (res != _taintSources.end())
		return res->second;

	return _FUNCTION_NOT_SOURCE;
}

void CTaintAnalysis::setDiff(set<Value *> &A,
                             set<Value *> &B, 
                             set<Value *> &AMinusB) {
    AMinusB.clear();
    for(set<Value *>::iterator ps = A.begin(), Eps = A.end();
            ps != Eps;
            ++ps) {
        Value *e = *(ps);
        if (0 == B.count(e))
            AMinusB.insert(e);
      }
}

bool CTaintAnalysis::merge(BasicBlock *curBB, BasicBlock *succBB) {
	set<Value *> &curBBOut = _IN[&curBB->back()];
	set<Value *> &succBBIn = _OUT[&succBB->front()];

	set<Value *> inDiff;
	setDiff(curBBOut, succBBIn, inDiff);
	
        if (!inDiff.empty())
	  succBBIn.insert(inDiff.begin(), inDiff.end());
	 
	return (inDiff.size() > 0);
}

inline void CTaintAnalysis::mergeCopyPredOutFlowToInFlow(Instruction &predInst, Instruction &curInst) {
  _IN[&curInst].insert(_OUT[&predInst].begin(), _OUT[&predInst].end());
  _OUT[&curInst].insert(_IN[&curInst].begin(), _IN[&curInst].end());
}

/**
 * Given a value v, and a DSGraph dsg (a DSGraph
 * represents the aliasing relationship within a function),
 * this method checks if v has aliases in function F with
 * dsg as DSGraph.
 */
void CTaintAnalysis::getAliases(Value *v,
								DSGraph *dsg,
								vector<Value *> &aliases) {

	if (dsg && dsg->hasNodeForValue(v)) {
		//If value v is sharing the same DSNode
		//as another value w, then mayAlias(v,w)
		DSNodeHandle &vHandle = dsg->getNodeForValue(v);
		DSGraph::ScalarMapTy &scalarMap = dsg->getScalarMap();

		DSGraph::ScalarMapTy::iterator sIt = scalarMap.begin();
		DSGraph::ScalarMapTy::iterator EsIt = scalarMap.end();

		const Value *vAlias = 0;
		while(sIt != EsIt) {
			DSNodeHandle &cur = (*sIt).second;
			if (cur == vHandle) {
				vAlias = (*sIt).first;
				if (vAlias && v != vAlias)
					aliases.push_back(const_cast<Value *>(vAlias));
			}
			++sIt;
		}
	}
}

/**
 * Adds value 'v' to the set OUT[I], denoting that instruction
 * I taints value 'v'.
 */
void CTaintAnalysis::insertToOutFlow(Instruction *I, Value *v) {
  int n = _OUT[I].count(v);
  if (0 == n) {
	Function *f = I->getParent()->getParent();
	DSGraph *dsg = _functionToDSGraph[f];
	insertToOutFlow(I, v, dsg);
  }
}

void CTaintAnalysis::insertToOutFlow(Instruction *I, Value *v, DSGraph *dsg) {

  assert(I && "The instruction to which an outflow is to be added must be non null!");
  assert(v && "The value to insert in the outflow of an instruction must be non null!");

		_OUT[I].insert(v);
		v->print(errs()); errs() << " gets tainted\n";

		/**
		 * Mark all aliases of v as tainted.
		 */
		if (dsg) {
			vector<Value *> vAliases;
			getAliases(v, dsg, vAliases);
			int s = vAliases.size();
			for(int k = 0; k < s; ++k) {
				_OUT[I].insert(vAliases[k]);
				vAliases[k]->print(errs());
			        errs() << " \t also gets tainted\n";
			}
		}
}

bool CTaintAnalysis::calls(Function *caller, Function *callee) {
	  CallGraphNode *cn = (*_cg)[caller];
	  if (cn) {
	    Function *curCallee = 0;
	    for(CallGraphNode::iterator pn = cn->begin(), Epn = cn->end();
		pn != Epn;
		++pn) {
	      curCallee = (*pn).second->getFunction();
	      if (curCallee == callee) return true;
	    }
	  }
	  return false;
	}

/**
 * Returns 'true' if value 'v' is tainted at the program point before
 * instruction 'I'.
 */
bool CTaintAnalysis::isValueTainted(Instruction *I, Value *v) {
	return ( 0 < _IN[I].count(v) );
}

void CTaintAnalysis::visit(Instruction &I) {
  if (_predInst)
    mergeCopyPredOutFlowToInFlow(*_predInst, I);
  //errs() << "## pred: "; _predInst->print(errs()); errs()<<"\n";
  //printIn(I);
  _super->visit(I);
  _predInst = &I;
}

void CTaintAnalysis::visitLoadInst(LoadInst &I) {
	errs() << "LOAD [p=*q]: ";
	I.print(errs());
	errs() << "\n";

	Value *q = I.getPointerOperand();

	if ( isValueTainted(&I, q)) {
		insertToOutFlow(&I, &I);
	}
}

void CTaintAnalysis::visitStoreInst(StoreInst &I)
{
	errs() << "STORE [*p=q]: ";
	I.print(errs());
	errs() << "\n";

	Value *q = I.getValueOperand();
	Value *p = I.getPointerOperand();

	if ( isValueTainted(&I, q)) {
		insertToOutFlow(&I, p);
	}
}

void CTaintAnalysis::visitCallInst(CallInst & I)
{
	errs() << "CALL [call func]: ";
	I.print(errs());
	errs() << "\n";

	Function *callee = I.getCalledFunction();

	//TODO: Check why some call statements may have a null callee
	if (!callee) {
		errs() << I.getParent()->getParent()->getName()
			   << " has no callee at: "; I.print(errs()); errs() << " !\n";
		return;
	}

	string calleeName = callee->getName().str();
	int arg_pos = isTaintSource(calleeName);

	if ( _FUNCTION_NOT_SOURCE == arg_pos ) return;

	int maxParams = I.getNumArgOperands();

	if ( _SOURCE_ARG_INVALID_MIN < arg_pos && arg_pos < maxParams ) {
		Value *taintedArg = 0;
		if (_SOURCE_ARG_RET == arg_pos)
		  taintedArg = &I;
		else
		  taintedArg = I.getArgOperand(arg_pos);
		insertToOutFlow(&I, taintedArg);
		//printOut(I);
		errs() << "Found a source " << calleeName << " with arg_pos " << arg_pos << "\n";
		
		/*for (Value::use_iterator u = taintedArg->use_begin(), 
		    e = taintedArg->use_end(); u != e; 
		    ++u) {
		  Value *aTaintedUse = *u;
		  if (Instruction *I = dyn_cast<Instruction>(aTaintedUse)) {
		        //errs() << "##Setting: "; aTaintedUse->print(errs()); errs() << " as tainted:\n";			
			insertToOutFlow(I, aTaintedUse); 
		  }
		}*/
	}
	else {
		std::ostringstream msg;
		msg << "Invalid argument position (" << arg_pos << ")"
			<< " max parameters is " << maxParams;
		log(msg.str());
	}
}

void CTaintAnalysis::visitCallInstInter(CallInst &I,
					Function *caller,
					Function *callee)
{
	errs() << "INTER CALL [call func]: ";
	I.print(errs());
	errs() << "\n";

	//if (!visitFunction) return;

	if (!callee) {
	  //The callee must be non null at all time
	  return;
	}

	if (caller && callee == caller) {
	  //We don't handle recursive functions at this point
	  return ;
	}

	if (!calls(caller, callee)) {
	  errs() << "## " << caller->getName() << " doesn't call "
	         << callee->getName() << " in our callgraph \n";
	  return ;
	}

	
	//Now copy call aruguments taint information into
	//the callee formal parameter taint information
	int argNr = I.getNumArgOperands();
	vector<bool> *calleeFormals = _summaryTable[callee];
	if (calleeFormals) {
	  Instruction &calleeFirstI = callee->front().front();
	  Function::arg_iterator pf = callee->arg_begin(), Epf = callee->arg_end();

	  
	  int k = 0;
	  Value *curArg;
	  while (pf != Epf && k < argNr) {
	    Argument &fml = (*pf);
	  //for (int k = 0; k < argNr; ++k) {
	    curArg = I.getArgOperand(k);
	    //If the call argument is tainted and the callee's corresponding formal
	    //parameter has not already been marked as tainted by previous
	    //analyzes (e.g. intraprocedural), then copy call arguments taint
	    //information into the callee.
	    if ( isValueTainted(&I, curArg) && !(*calleeFormals)[k] )
	      _IN[&calleeFirstI].insert(&fml); 
	    ++pf;
	    ++k;
	  }

	  //errs() << "## Before callee analysis\n";
	  Instruction &calleeLastI = callee->back().back();
	   //errs() << "## Before INTER\n";
	  //printIn(calleeFirstI);
	  //errs() << "## Start INTER\n";

	  //Now analyze the callee
	  _predInst = &I;
	  _super->visit(*callee);
	  //errs() << "## Print after INTER\n";
	  //printOut(calleeLastI);
	  //errs() << "## After INTER\n";
	  //errs() << "## Before INTER\n";
	  
	  set<Value *> outDiff;
	  setDiff(_OUT[&calleeLastI], _IN[&calleeFirstI], outDiff);

	  DSGraph * calleeDSG = _functionToDSGraph[callee];
	  vector<Value *> fmlAliases; 
	  pf = callee->arg_begin();
	  Epf = callee->arg_end();
	  k = 0; //To iterate over vector calleeFormals
	  curArg = 0;
	  while (pf != Epf && k < argNr) {
	    Argument &fml = (*pf);
	    curArg = I.getArgOperand(k);
	    getAliases(&fml, calleeDSG, fmlAliases);
	    fmlAliases.insert(fmlAliases.begin(), &fml);
	    for(unsigned j = 0; j < fmlAliases.size(); ++j) {
	      if ( outDiff.count(fmlAliases[j]) > 0 ) {
	        setProcArgTaint(callee, k, true);
	        if ( !isValueTainted(&I, curArg) )
	          insertToOutFlow(&I, curArg);
	        break;
	      }
	    }
	    ++pf;
	    ++k;
	  }
	  //errs() << "## After callee analysis\n";
	}
}

void CTaintAnalysis::visitReturnInst(ReturnInst &I) {
	Function *F = I.getParent()->getParent();
	errs() << "Analyzing return instruction for " << F->getName() << "\n";
	Value *retVal = I.getReturnValue();

	//TODO: check if this could happen
	if (!retVal){
	  errs() << "No return value for: " << F->getName() << "\n";
	  return;
	}

	errs() << "\t";
	retVal->print(errs());
        errs()<< "\n";
	
	//printIn(I);

	if (isValueTainted(&I, retVal)) {
	  errs() << "\tis tainted\n";
	  setProcArgTaint(F, F->getNumOperands()+1, true);
	}
}

static RegisterPass<CTaintAnalysis>
X("ctaintmod", "CTaint Module Pass",
		false /* Only looks at CFG */,
		true /* Analysis Pass */);

