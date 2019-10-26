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
#include "llvm/Analysis/ValueTracking.h"

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

    template <typename T, unsigned N>
    void calcPreOrder(SmallVector<T, N> &preOrderBBs) {

    }

    /**
     * check whether hoisting the instruction is safe or not.
     * @I: target instruction
     */
    bool safeToHoist(Instruction &I, Loop *L, DominatorTree &domTree) {
      /*
       * Hoisting conditions.
       *
       * e.g. d:t=x+y
       *
       * 1) d's block dominates all loop exits at which t is live-out, and
       * 2) there is only one definition of t in the loop, and
       * 3) t is not live-out of the pre-header.
       *
       * dominate() handles all the above situations.
       */
      bool isSafeToHoist = true;;
      SmallVector<BasicBlock *, 10> exitBlocks;

      if (llvm::isSafeToSpeculativelyExecute(&I)) { return true; }

      L->getExitBlocks(exitBlocks);
      for (BasicBlock* exitBlock : exitBlocks) {
        if (!domTree.dominates(&I, exitBlock)) {
          isSafeToHoist = false;
          break;
        }
      }

      return isSafeToHoist;
    }

    /**
     * check whether the instruction is invariant or not.
     * @I: target instruction.
     */
    bool isLoopInvariant(Instruction &I, Loop *L) {
      /*
       * Loop-invariant conditions.
       *
       * e.g. t=x+y
       *
       * 1) x and y are constants, or
       * 2) all reaching definitions of x and y are
       *    outside the loop, or
       * 3) only one definition reaches x(or y), and
       *    that definition is loop-invarinat.
       * But maybe it is not necessary.
       * We can exploit hasLoopInvariantOperands().
       */
      // check instruction types.
      if (!I.isBinaryOp() && !I.isShift() && !I.isCast()
          && !llvm::SelectInst::classof(&I)
          && !llvm::GetElementPtrInst::classof(&I)) { return false; }

      /* I tried to check the below instructions too,
       * but I noticed that malloc or alloca etc is hard to detect in IR level.
       * So, just keep the above conditions. It seems correct.
      if (llvm::TerminatorInst::classof(I) || llvm::PHINode::classof(I)
          || I.isLoadOrStor() || llvm::CallInst::classof(I) ||
          llvm::InvokeInst::classof(I) || ..
          */

      return L->hasLoopInvariantOperands(&I);
    }

    /**
     * Loop invariant code motion.
     * Each loop object gives us a preheader block for the loop:
     * LoopSimplify pass does it.
     * @L: loop.
     */
    void LICM(Loop *L, LoopInfo &LInfo, DominatorTree &domTree) {
      BasicBlock *preHeaderBB = L->getLoopPreheader();
      // Iterate each basic block BB dominated by loop header, in pre-order
      // on dominator tree.
      for (BasicBlock* BB : L->blocks()) { // not in an inner loop or outside L
        errs() << "BasicBlock:" << BB->getName();
        if (LInfo.getLoopFor(BB) == L) { // only consider not-nested loops' BB
          for (Instruction &instr : *BB) {
            errs() << instr.getOpcodeName() << "\n";
            if (isLoopInvariant(instr, L) && safeToHoist(instr, L, domTree)) {
              instr.moveBefore(preHeaderBB->getTerminator());
              // move I to pre-header basic-block;
            }
          }
        }
        errs() << "Next.\n\n";
      }
    }

    bool runOnLoop(Loop *L, LPPassManager &LPW) {
      // Note that LoopPass iterates all loops including nested loops.
      LoopInfo &LInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      DominatorTreeWrapperPass &domTreeWrapPass =
                            getAnalysis<DominatorTreeWrapperPass>();
      DominatorTree &domTree = domTreeWrapPass.getDomTree();
      SmallVector<BasicBlock *, 10> preOrderedBBs;

      calcPreOrder(preOrderedBBs);
      LICM(L, LInfo, domTree);
      return true;
    }
	}; // end of struct HL26847
}  // end of anonymous namespace

char HL26847::ID = 0;

static RegisterPass<HL26847> X("HL26847", "HL26847 Pass",
		false /* Only looks at CFG */,
		false /* Analysis Pass */);
