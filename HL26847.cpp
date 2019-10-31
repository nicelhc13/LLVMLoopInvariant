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
//#define PRINT_LOG
#define PRINT_LICM

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
#ifdef PRINT_LOG
      errs() << I << "\n";
#endif
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
          && !SelectInst::classof(&I)
          && !GetElementPtrInst::classof(&I)) { return false; }

      bool checkLoopInvariant = true;

      // 1) Check whether all operands are constant.
      //    If is not, checkLoopInvariant is set to 'false'.
      size_t numOperands = I.getNumOperands();
#ifdef PRINT_LOG
      errs() << "# of operands:" << numOperands << "\n";
#endif
      for (uint32_t opi = 0; opi < numOperands; opi++) {
        if (!Constant::classof(I.getOperand(opi))) {
          checkLoopInvariant = false;
#ifdef PRINT_LOG
          errs() << "Operand(" << opi << ") is not constant\n";
#endif
        }
#ifdef PRINT_LOG
        else {
          errs() << "Operand(" << opi << ") is constant\n";
        }
#endif
      }

      // 2) Check whether all operands are compuated outside the loop.
      //    If it is, return value is 'false'.
      checkLoopInvariant = checkLoopInvariant || L->hasLoopInvariantOperands(&I);
      return checkLoopInvariant;
    }

    /**
     * Loop invariant code motion.
     * Each loop object gives us a preheader block for the loop:
     * LoopSimplify pass does it.
     * @L: loop.
     */
    bool LICM(Loop *L, LoopInfo &LInfo,
              DominatorTree &domTree, DomTreeNode *curNode) {
      bool isDomTreeChanged = false;
      BasicBlock *preHeaderBB = L->getLoopPreheader();
      BasicBlock *loopHeader = L->getHeader();
      BasicBlock *curBB = curNode->getBlock();

      // Iterate each basic block BB dominated by loop header, in pre-order
      // on dominator tree.

      if (domTree.dominates(loopHeader, curBB)) {
        if (LInfo.getLoopFor(curBB) == L) { // only consider not-nested loops' BB
          auto instrIter = curBB->getInstList().begin();
          for (; instrIter != curBB->getInstList().end();) {
            Instruction &instr = *(instrIter++);
            if (isLoopInvariant(instr, L) && safeToHoist(instr, L, domTree)) {
              // move I to pre-header basic-block;
              instr.moveBefore(preHeaderBB->getTerminator());
              isDomTreeChanged = true;
              errs() << instr << "\n";
            }
          }
        }
      }

      for (DomTreeNode *children : curNode->getChildren()) {
        isDomTreeChanged = isDomTreeChanged || LICM(L, LInfo, domTree, children);
      }

      return isDomTreeChanged;
    }

    bool runOnLoop(Loop *L, LPPassManager &LPW) {
      // Note that LoopPass iterates all loops including nested loops.
      bool isDomTreeChanged = true;
      LoopInfo &LInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      DominatorTreeWrapperPass &domTreeWrapPass =
                            getAnalysis<DominatorTreeWrapperPass>();
      DominatorTree &domTree = domTreeWrapPass.getDomTree();
      isDomTreeChanged = LICM(L, LInfo, domTree, domTree.getRootNode());
#ifdef PRINT_LOG
      errs() << "========START======\n";;
      for (BasicBlock* BB : preOrderedBBs) {
        if (LInfo.getLoopFor(BB) == L) {
          errs() << "BasicBlock:\n";
          for (Instruction &instr : *BB) {
            errs() << instr.getOpcodeName() << "\n";
          }
          errs() << "\n";
        }
      }
      errs() << "===================\n";;
#endif
      return isDomTreeChanged;
    }
	}; // end of struct HL26847
}  // end of anonymous namespace

char HL26847::ID = 0;

static RegisterPass<HL26847> X("HL26847", "HL26847 Pass",
                false /* Only looks at CFG */,
                false /* Analysis Pass */);
