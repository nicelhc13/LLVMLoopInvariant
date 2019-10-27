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

    template <typename T, unsigned N>
    void calcPreOrder(SmallVector<T, N> &preOrderBBs,
                      Loop* L, DominatorTree &domTree,
                      DomTreeNode *curNode) {
      BasicBlock *loopHeader = L->getHeader();
      BasicBlock *curBB = curNode->getBlock();
      if (domTree.dominates(loopHeader, curBB)) {
        preOrderBBs.emplace_back(curBB);
      }

      for (DomTreeNode *children : curNode->getChildren()) {
        calcPreOrder(preOrderBBs, L, domTree, children);
      }
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

      errs() << "safeToHoist:" << isSafeToHoist << "\n";
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

      /* I tried to check the below instructions too,
       * but I noticed that malloc or alloca etc is hard to detect in IR level.
       * So, just keep the above conditions. It seems correct.
      if (llvm::TerminatorInst::classof(I) || llvm::PHINode::classof(I)
          || I.isLoadOrStor() || llvm::CallInst::classof(I) ||
          llvm::InvokeInst::classof(I) || ..
          */

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
      errs() << "Loop invariant: " << checkLoopInvariant << "\n";
      checkLoopInvariant |= L->hasLoopInvariantOperands(&I);
      errs() << "HasLoopInvariantOperands: " << 
                 L->hasLoopInvariantOperands(&I) << "\n";
      return checkLoopInvariant;
    }

    /**
     * Loop invariant code motion.
     * Each loop object gives us a preheader block for the loop:
     * LoopSimplify pass does it.
     * @L: loop.
     */
    template <typename T, unsigned N>
    bool LICM(Loop *L, LoopInfo &LInfo,
              DominatorTree &domTree, SmallVector<T, N> &preOrderBBs) {
      bool isDomTreeChanged = false;
      BasicBlock *preHeaderBB = L->getLoopPreheader();
      // Iterate each basic block BB dominated by loop header, in pre-order
      // on dominator tree.

#ifdef PRINT_LOG
      errs() << "========START======\n";;
#endif
      //for (BasicBlock* BB : L->blocks()) { //not in an inner loop or outside L
      for (BasicBlock* BB : preOrderBBs) {
        if (LInfo.getLoopFor(BB) == L) { // only consider not-nested loops' BB
#ifdef PRINT_LOG
          errs() << "BasicBlock\n";
#endif
          for (Instruction &instr : *BB) {
#ifdef PRINT_LOG
            errs() << instr.getOpcodeName() << "\n";
#endif
            if (isLoopInvariant(instr, L) && safeToHoist(instr, L, domTree)) {
              // move I to pre-header basic-block;
              instr.moveBefore(preHeaderBB->getTerminator());
              isDomTreeChanged = true;
#ifdef PRINT_LOG
              errs() << "Hoisted..\n";
              errs() << instr << "\n";
#endif
            }
          }
#ifdef PRINT_LOG
          errs() << "\n";
#endif
        }
      }
#ifdef PRINT_LOG
      errs() << "===================\n";;
#endif
      return isDomTreeChanged;
    }

    bool runOnLoop(Loop *L, LPPassManager &LPW) {
      // Note that LoopPass iterates all loops including nested loops.
      bool isDomTreeChanged = true;
      LoopInfo &LInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      DominatorTreeWrapperPass &domTreeWrapPass =
                            getAnalysis<DominatorTreeWrapperPass>();
      DominatorTree &domTree = domTreeWrapPass.getDomTree();
      SmallVector<BasicBlock *, 10> preOrderedBBs;

      calcPreOrder(preOrderedBBs, L, domTree, domTree.getRootNode());
      isDomTreeChanged = LICM(L, LInfo, domTree, preOrderedBBs);
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
