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

    /**
     * check whether hoisting the instruction is safe or not.
     * @I: target instruction
     */
    bool safeToHoist(Instruction &I) {
      bool isSafeToHoist = false;

      return isSafeToHoist;
    }

    /**
     * check whether the instruction is invariant or not.
     * @I: target instruction.
     */
    bool isLoopInvariant(Instruction &I) {
      bool isInvariant = false;

      return isInvariant;
    }

    /**
     * Loop invariant code motion.
     * Each loop object gives us a preheader block for the loop:
     * LoopSimplify pass does it.
     * @L: loop.
     */
    void LICM(Loop *L) {
      // Iterate each basic block BB dominated by loop header, in pre-order
      // on dominator tree.
      for (BasicBlock* BB : L->blocks()) { // not in an inner loop or outside L
        for (Instruction &instr : *BB) {
          if (isLoopInvariant(instr) && safeToHoist(instr)) {
            // move I to pre-header basic-block;
          }
        }
      }
    }

    bool runOnLoop(Loop *L, LPPassManager &LPW) {
      LICM(L);
      return false;
    }
	}; // end of struct HL26847
}  // end of anonymous namespace

char HL26847::ID = 0;

static RegisterPass<HL26847> X("HL26847", "HL26847 Pass",
		false /* Only looks at CFG */,
		false /* Analysis Pass */);
