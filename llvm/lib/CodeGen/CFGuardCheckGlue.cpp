//===- CFGuardCheckGlue.cpp - Glue CFG check and idirect call together ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "cfguardcheck"

namespace {

class CFGuardGlueImpl {
  bool regOverlaps(Register R, const SmallVector<Register> &Vec);
  SmallVector<Register> getRegUses(MachineInstr &MI);
  const uint32_t *getCallRegMask(MachineInstr &MI);
  bool glueCFGCheckAndCall(MachineInstr *CMI,
                           const MachineBasicBlock::iterator &Target);

public:
  CFGuardGlueImpl(MachineFunction &MF) : MF(MF) {
    TRI = MF.getSubtarget().getRegisterInfo();
  }

  bool run();

  MachineFunction &MF;
  const TargetRegisterInfo *TRI;
};

bool CFGuardGlueImpl::regOverlaps(Register R,
                                  const SmallVector<Register> &Vec) {
  return llvm::any_of(Vec, [&](auto &Reg) { return TRI->regsOverlap(Reg, R); });
}

SmallVector<Register> CFGuardGlueImpl::getRegUses(MachineInstr &MI) {
  SmallVector<Register> Uses;
  for (const MachineOperand &MO : MI.operands())
    if (MO.isReg() && MO.isUse())
      Uses.push_back(MO.getReg());
  return Uses;
}

const uint32_t *CFGuardGlueImpl::getCallRegMask(MachineInstr &MI) {
  assert(MI.isCall());
  for (const MachineOperand &MO : MI.operands())
    if (MO.isRegMask())
      return MO.getRegMask();
  return nullptr;
}

bool CFGuardGlueImpl::glueCFGCheckAndCall(
    MachineInstr *CMI, const MachineBasicBlock::iterator &Target) {

  auto CMIIt = CMI->getIterator();
  if (std::next(CMIIt) == Target)
    return false;

  const uint32_t *CheckRegMask = getCallRegMask(*CMI);
  const uint32_t *CallRegMask = getCallRegMask(*Target);
  assert(CheckRegMask && CallRegMask);
  auto I = CMI->getIterator();
  ++I;
  SmallVector<Register> ImmutableRegs = TRI->getCFGuardCheckImmutableRegs();
  SmallVector<Register> CallUses = getRegUses(*Target);
  for (; I != Target; ++I) {
    auto &MI = *I;
    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg())
        continue;
      Register R = MO.getReg();
      assert(R.isPhysical());

      if (llvm::is_contained(ImmutableRegs, R))
        continue;
      MCPhysReg MCR = R.asMCReg();
      if (TRI->isCFGuardCheckArgumentRegister(MCR))
        return false;
      if (MachineOperand::clobbersPhysReg(CheckRegMask, MCR)) {
        if (regOverlaps(R, CallUses))
          return false;
        // Register survives second call, but not first
        if (!MachineOperand::clobbersPhysReg(CallRegMask, MCR))
          return false;
      }
    }
  }

  auto *MBB = CMI->getParent();
  MBB->splice(Target, MBB, CMIIt);
  return true;
}

bool CFGuardGlueImpl::run() {
  Module *M = MF.getFunction().getParent();
  ControlFlowGuardMode CFGM = M->getControlFlowGuardMode();
  if (CFGM != ControlFlowGuardMode::Enabled)
    return false;
  std::vector<MachineInstr *> Checks;
  for (auto &MBB : MF)
    for (auto &MI : MBB)
      if (MI.getFlag(MachineInstr::CFGuardCheck))
        Checks.push_back(&MI);

  bool Changed = false;
  for (auto *CheckMI : Checks) {
    auto I = CheckMI->getIterator();
    auto E = CheckMI->getParent()->end();
    ++I;
    for (; I != E; ++I) {
      if (I->isCall())
        Changed |= glueCFGCheckAndCall(CheckMI, I);
    }
  }
  return Changed;
}
} // end anonymous namespace

class CFGuardCheckGlueLegacy : public MachineFunctionPass {
public:
  static char ID;

  CFGuardCheckGlueLegacy() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "CFGuard check glue"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    CFGuardGlueImpl impl(MF);
    return impl.run();
  }
};

char CFGuardCheckGlueLegacy::ID = 0;

MachineFunctionPass *llvm::createCFGuardCheckGluePass() {
  return new CFGuardCheckGlueLegacy();
}

INITIALIZE_PASS_BEGIN(CFGuardCheckGlueLegacy, DEBUG_TYPE, "CFGuard check glue",
                      false, false)
INITIALIZE_PASS_END(CFGuardCheckGlueLegacy, DEBUG_TYPE, "CFGuard check glue",
                    false, false)
