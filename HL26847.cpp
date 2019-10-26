#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Utils.h"

#include <iostream>

#define STR(s) std::to_string(s)

using namespace llvm;

/**
 * NOTE
 * You can modify structure name as your UT EID
 */
namespace {
	struct HL26847 : public LoopPass {
		static char ID;

		HL26847() : LoopPass(ID) { }

		void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
      /**
       * LoopSimplifyID-
       * Insert Pre-header blocks into the CFG for every function 
       * in the module. This pass updates dominator information, loop informat
       * -ion, and does not add critical edges to the CFG
       * https://llvm.org/doxygen/Transforms_2Utils_8h_source.html 
       */
      AU.addRequiredID(LoopSimplifyID); /* Preheader and other transforms */
			AU.addRequired<LoopInfoWrapperPass>(); /* preserve loop pass */
      // Need dominators for code motion
      AU.addRequired<DominatorTreeWrapperPass>();
		}

    bool runOnLoop(Loop *, LPPassManager &LPW) {

      return false;
    }
	}; // end of struct HL26847
}  // end of anonymous namespace

char HL26847::ID = 0;

static RegisterPass<HL26847> X("HL26847", "HL26847 Pass",
		false /* Only looks at CFG */,
		false /* Analysis Pass */);
