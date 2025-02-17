/*
 * GenMC -- Generic Model Checking.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-3.0.html.
 *
 * Author: Michalis Kokologiannakis <michalis@mpi-sws.org>
 */

#include "config.h"
#include "EliminateCastsPass.hpp"
#include "Error.hpp"
#include "VSet.hpp"

#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#if defined(HAVE_LLVM_TRANSFORMS_UTILS_H)
#include <llvm/Transforms/Utils.h>
#endif
#include <llvm/Transforms/Utils/PromoteMemToReg.h>

using namespace llvm;

/* Just be lazy and under-approximate this; it should not be that important */
#ifdef LLVM_HAS_ONLYUSEDBYLIFETIMEMARKERSORDROPPABLEINSTS
# define IS_LIFETIME_START_OR_END(i) isLifetimeStartOrEnd(i)
# define IS_DROPPABLE(i) isDroppable(i)
# define ONLY_USED_BY_MARKERS_OR_DROPPABLE(i) onlyUsedByLifetimeMarkersOrDroppableInsts(i)
#else
# define IS_LIFETIME_START_OR_END(i)				\
	(i->getIntrinsicID() != Intrinsic::lifetime_start &&	\
	 i->getIntrinsicID() != Intrinsic::lifetime_end)
# define IS_DROPPABLE(i) false
# define ONLY_USED_BY_MARKERS_OR_DROPPABLE(i) onlyUsedByLifetimeMarkers(i)
#endif

#ifdef LLVM_HAS_ALIGN
# define GET_INST_ALIGN(i) i->getAlign()
#else
# define GET_INST_ALIGN(i) i->getAlignment()
#endif

#ifdef LLVM_HAS_GET_SYNC_SCOPE_ID
# define GET_INST_SYNC_SCOPE(i) i->getSyncScopeID()
#else
# define GET_INST_SYNC_SCOPE(i) i->getSynchScope()
#endif

#ifdef LLVM_EXECUTIONENGINE_DATALAYOUT_PTR
# define GET_TYPE_ALLOC_SIZE(M, x)		\
	(M).getDataLayout()->getTypeAllocSize((x))
#else
# define GET_TYPE_ALLOC_SIZE(M, x)		\
	(M).getDataLayout().getTypeAllocSize((x))
#endif

void EliminateCastsPass::getAnalysisUsage(AnalysisUsage &AU) const
{
	AU.addRequired<AssumptionCacheTracker>();
	AU.addRequired<DominatorTreeWrapperPass>();
	AU.setPreservesCFG();
}

static bool haveSameSizePointees(const Type *p1, const Type *p2, const DataLayout &DL)
{
	auto *pTy1 = dyn_cast<PointerType>(p1);
	auto *pTy2 = dyn_cast<PointerType>(p2);

	return pTy1 && pTy2 && DL.getTypeAllocSize(pTy1->getElementType()) ==
		DL.getTypeAllocSize(pTy2->getElementType());
}

static bool isUnpromotablePureHelper(User *u, std::vector<Instruction *> &aliases);

/* Return whether u is pure; if it is, it will be returned in ALIASES with its aliases */
static bool isUserPure(User *u, std::vector<Instruction *> &aliases)
{
	/* We only allow direct, non-volatile loads and stores, as well as "pure"
	 * bitcasts (only used by direct, non-volatile loads and stores).
	 * (Atomicity qualifiers are not important as they have no meaning for local
	 * allocas.) */
	if (auto *li = dyn_cast<LoadInst>(u)) {
		return !li->isVolatile();
	} else if (auto *si = dyn_cast<StoreInst>(u)) {
		/* Don't allow a store OF the U, only INTO the U */
		return !si->isVolatile() && si->getOperand(0) != aliases.back();
	} else if (auto *ii = dyn_cast<IntrinsicInst>(u)) {
		return IS_LIFETIME_START_OR_END(ii) || IS_DROPPABLE(ii);
	} else if (auto *bci = dyn_cast<BitCastInst>(u)) {
		if (ONLY_USED_BY_MARKERS_OR_DROPPABLE(bci))
			return true;
		if (!haveSameSizePointees(bci->getSrcTy(), bci->getDestTy(), bci->getModule()->getDataLayout()))
			return false;
		aliases.push_back(bci);
		return isUnpromotablePureHelper(bci, aliases);
	} else if (auto *gepi = dyn_cast<GetElementPtrInst>(u)) {
		return gepi->hasAllZeroIndices() && ONLY_USED_BY_MARKERS_OR_DROPPABLE(gepi);
	} else if (auto *asci = dyn_cast<AddrSpaceCastInst>(u)) {
		return onlyUsedByLifetimeMarkers(asci);
	}
	/* All other cases are not safe*/
	return false;
}

static bool isUnpromotablePureHelper(User *i, std::vector<Instruction *> &aliases)
{
	return std::all_of(i->users().begin(), i->users().end(),
			   [&aliases](User *u){ return isUserPure(u, aliases); });
}

static bool isUnpromotablePure(Instruction *i, std::vector<Instruction *> &aliases)
{
	aliases.push_back(i);
	auto pure = isUnpromotablePureHelper(i, aliases);
	if (!pure)
		aliases.clear();
	return pure;
}

static Instruction::CastOps
isEliminableCastPair(const CastInst *CI1, const CastInst *CI2, const DataLayout &DL)
{
	Type *SrcTy = CI1->getSrcTy();
	Type *MidTy = CI1->getDestTy();
	Type *DstTy = CI2->getDestTy();

	Instruction::CastOps firstOp = CI1->getOpcode();
	Instruction::CastOps secondOp = CI2->getOpcode();
	Type *SrcIntPtrTy =
		SrcTy->isPtrOrPtrVectorTy() ? DL.getIntPtrType(SrcTy) : nullptr;
	Type *MidIntPtrTy =
		MidTy->isPtrOrPtrVectorTy() ? DL.getIntPtrType(MidTy) : nullptr;
	Type *DstIntPtrTy =
		DstTy->isPtrOrPtrVectorTy() ? DL.getIntPtrType(DstTy) : nullptr;
	unsigned Res = CastInst::isEliminableCastPair(firstOp, secondOp, SrcTy, MidTy,
						      DstTy, SrcIntPtrTy, MidIntPtrTy,
						      DstIntPtrTy);

	/* We don't want to form an inttoptr or ptrtoint that converts to an integer
	 * type that differs from the pointer size */
	if ((Res == Instruction::IntToPtr && SrcTy != DstIntPtrTy) ||
	    (Res == Instruction::PtrToInt && DstTy != SrcIntPtrTy))
		Res = 0;

	return Instruction::CastOps(Res);
}

/* Returns true BCI's soure is a constant or a zero-indices GEP */
static bool isConstantOrGEPCast(const BitCastInst *bci)
{
	if (isa<Constant>(bci->getOperand(0)))
	    return true;

	auto *gepi = dyn_cast<GetElementPtrInst>(bci->getOperand(0));
	return (gepi && gepi->hasAllZeroIndices() && llvm::isa<Constant>(gepi->getPointerOperand()));
}

/* Returns whether LI loads from a pointer produced by a ptrtoptr cast with source type TYP */
static Value *isLoadConstCastFromType(const LoadInst *li, const PointerType *typ, const DataLayout &DL)
{
	auto *M = li->getModule();

	/* We capture two basic cases for now:
	 * 1) LI loading from a bitcast from const, and
	 * 2) LI loading from a constexpr */
	if (auto *lci = dyn_cast<BitCastInst>(li->getPointerOperand())) {
		auto *srcTy = lci->getSrcTy();
		auto *dstTy = lci->getDestTy();
		/* The cast needs come from a constant and preserve the size */
		if (isConstantOrGEPCast(lci) && srcTy == typ && haveSameSizePointees(srcTy, dstTy, DL))
			return lci->getOperand(0);
		return nullptr;
	}
	if (auto *lce = dyn_cast<ConstantExpr>(li->getPointerOperand())) {
		if (lce->isCast() && lce->getOpcode() == Instruction::CastOps::BitCast) {
			auto *srcTy = lce->getOperand(0)->getType();
			if (srcTy == typ->getElementType())
				return lce->getOperand(0);
		}
		return nullptr;
	}
	return nullptr;
}

static void replaceAndMarkDelete(Instruction *toDel, Value *toRepl,
			  VSet<Instruction *> *deleted = nullptr)
{
	toDel->replaceAllUsesWith(toRepl);
	toDel->eraseFromParent();
	if (deleted)
		deleted->insert(toDel);
}

/* Performs some common transformations on the cast and deletes it, if necessary.
 * Returns true if the transformation succeeded (and the caller should delete CI),
 * and sets TRANSFORMED to its replacement. */
static bool commonCastTransforms(CastInst &ci, std::vector<Instruction *> &aliases,
				 VSet<Instruction *> &deleted)
{
	const DataLayout &DL = ci.getModule()->getDataLayout();
	Value *src = ci.getOperand(0);
	Type *typ = ci.getType();

	/* Eliminate zero-use casts */
	if (ci.hasNUses(0)) {
		replaceAndMarkDelete(&ci, nullptr, &deleted);
		return true;
	}

	/* Eliminate same-type casts */
	if (ci.getSrcTy() == ci.getDestTy()) {
		replaceAndMarkDelete(&ci, ci.getOperand(0), &deleted);
		return true;
	}

	/* Try to eliminate a cast of a cast */
	if (auto *csrc = dyn_cast<CastInst>(src)) { /* A->B->C cast */
		if (Instruction::CastOps newOpc = isEliminableCastPair(csrc, &ci, DL)) {
			/* The first cast (csrc) is eliminable so we need to fix up or replace
			 * the second cast (ci). csrc will then have a good chance of being dead */
			auto *res = CastInst::Create(newOpc, csrc->getOperand(0), typ);
			/* Point debug users of the dying cast to the new one. */
			// if (CSrc->hasOneUse())
			// 	replaceAllDbgUsesWith(*CSrc, *Res, ci, DT);
			res->takeName(&ci);
			ci.getParent()->getInstList().insert(ci.getIterator(), res);
			replaceAndMarkDelete(&ci, res, &deleted);
			aliases.push_back(res);
			return true;
		}
	}

	/* Try to eliminate unnecessary casts */
	if (ci.hasOneUse()) {
		auto *cusr = *ci.users().begin();
		auto *si = dyn_cast<StoreInst>(cusr);
		if (!si || &ci == si->getValueOperand())
			return false;

		auto *li = dyn_cast<LoadInst>(si->getValueOperand());
		if (!li)
			return false;

		auto *origPTy = dyn_cast<PointerType>(ci.getOperand(0)->getType());
		BUG_ON(!origPTy);
		auto *lsrc = isLoadConstCastFromType(li, origPTy, DL);
		if (!lsrc)
			return false;

		/* If the load came from cast operand, mark it as an alias before we delete */
		if (auto *lci = dyn_cast<BitCastInst>(li->getPointerOperand()))
			if (lci->hasNUses(0))
				replaceAndMarkDelete(lci, nullptr);

		auto *load = new LoadInst(origPTy->getElementType(), lsrc, li->getName(),
					  li->isVolatile(), GET_INST_ALIGN(li), li->getOrdering(),
					  GET_INST_SYNC_SCOPE(li), si);
		auto *store = new StoreInst(load, ci.getOperand(0), si->isVolatile(), GET_INST_ALIGN(si),
					    si->getOrdering(), GET_INST_SYNC_SCOPE(si), si);

		replaceAndMarkDelete(si, store);
		replaceAndMarkDelete(li, load);

		/* At this point, the cast should have no users */
		BUG_ON(!ci.hasNUses(0));
		replaceAndMarkDelete(&ci, nullptr, &deleted);
		return true;
	}
	return false;
}

static bool eliminateAllocaCasts(std::vector<Instruction *> &aliases)
{
	if (aliases.empty())
		return false;

	bool eliminated = false;
	bool changed = true;

	VSet<Instruction *> deleted;
	while (changed) {
		changed = false;

		/* Skip the allocation itself */
		BUG_ON(!llvm::isa<AllocaInst>(aliases[0]));
		for (auto i = 1u; i < aliases.size(); i++) {
			/* If we have already deleted the instruction, skip */
			if (deleted.count(aliases[i]))
				continue;

			auto *bci = llvm::dyn_cast<BitCastInst>(aliases[i]);
			BUG_ON(!bci);

			bool transformed = commonCastTransforms(*bci, aliases, deleted);
			if (transformed) {
				changed = true;
				eliminated = true;
			}
		}
	}
	return eliminated;
}

static bool eliminateCasts(Function &F, DominatorTree &DT, AssumptionCache &AC)
{
	auto &eBB = F.getEntryBlock();
	bool changed = true;

	while (changed) {
		changed = false;
		/* Find allocas that are safe to promote (skip terminator) */
		for (auto it = eBB.begin(), e = --eBB.end(); it != e; ++it) {
			/* An alloca is promotable if all its users are "pure" */
			if (auto *ai = dyn_cast<AllocaInst>(it)) {
				std::vector<Instruction *> aliases;
				if (isUnpromotablePure(ai, aliases)) {
					changed = eliminateAllocaCasts(aliases);
				}
			}
		}
	}
	return changed;
}

bool EliminateCastsPass::runOnFunction(Function &F)
{
	auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
	auto &AC = getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);
	return eliminateCasts(F, DT, AC);
}

Pass *createEliminateCastsPass()
{
	auto *p = new EliminateCastsPass();
	return p;
}

char EliminateCastsPass::ID = 42;
static llvm::RegisterPass<EliminateCastsPass> P("elim-casts", "Eliminates certain forms of casts.");
