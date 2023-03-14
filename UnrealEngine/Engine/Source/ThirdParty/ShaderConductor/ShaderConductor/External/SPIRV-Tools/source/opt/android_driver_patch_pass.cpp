// Copyright (c) 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "android_driver_patch_pass.h"

#include <set>

#include "source/opt/instruction.h"
#include "source/opt/ir_context.h"

namespace spvtools {
namespace opt {

Pass::Status AndroidDriverPatchPass::Process() {
  bool modified = false;
  
  for (auto functionIter = get_module()->begin(); functionIter != get_module()->end(); ++functionIter) {
    Instruction* entryPoint = nullptr;

    functionIter->ForEachInst([this, &modified, &entryPoint](Instruction* inst) {
      if (entryPoint == nullptr) {
		switch(inst->opcode()) {
		  case SpvOpFunction:
		  case SpvOpFunctionParameter:
		  case SpvOpVariable:
		  case SpvOpLabel:
	        break;
		  default:
		    entryPoint = inst;
		}
      }

	  if (entryPoint) {
	    modified |= FixupOpPhiMatrix4x3(inst, entryPoint);
	    modified |= FixupOpVectorShuffle(inst);
        modified |= FixupOpVariableFunctionPrecision(inst);
	  }
    });
  }

  for (auto& val : get_module()->types_values()) {
    modified |= FixupOpTypeImage(&val);
  };

  return modified ? Status::SuccessWithChange : Status::SuccessWithoutChange;
}

void InsertAfterOpPhi(Instruction* curInst, Instruction* newInst) {
  // OpPhi Instructions must be the first instructions after a branch
  Instruction* nextAvailableNode = curInst;
  while (nextAvailableNode->NextNode()->opcode() == SpvOpPhi) {
    if (nextAvailableNode->NextNode()) {
      nextAvailableNode = nextAvailableNode->NextNode();
    } else {
      break;
    }
  }
  newInst->InsertAfter(nextAvailableNode);
}

bool AndroidDriverPatchPass::FixupOpPhiMatrix4x3(Instruction* inst,
                                                 Instruction* entryPoint) {
  if (inst->opcode() != SpvOpPhi) {
    return false;
  }

  // Only care about Matrix types
  Operand typeOp = inst->GetOperand(0);
  Instruction* typeInst = context()->get_def_use_mgr()->GetDef(typeOp.words[0]);
  if (typeInst->opcode() != SpvOpTypeMatrix) {
    return false;
  }

  // Now we know it's a matrix, check it's a at least a 3x3
  uint32_t ColumnCount = typeInst->GetSingleWordOperand(2);
  if (ColumnCount < 3) {
    return false;
  }

  // Check it's a vector3/4
  const Instruction* vectorType = context()->get_def_use_mgr()->GetDef(typeInst->GetSingleWordOperand(1));
  uint32_t vectorLen = vectorType->GetSingleWordOperand(2);
  if (vectorLen < 3) {
    return false;
  }

  // Check it's a float
  const Instruction* floatType = context()->get_def_use_mgr()->GetDef(vectorType->GetSingleWordOperand(1));
  if (floatType->opcode() != SpvOpTypeFloat) {
    return false;
  }

  uint32_t numArgs = inst->NumInOperands();
  uint32_t numOps = numArgs / 2;

  std::map<uint32_t, std::vector<uint32_t>> valueTypeComponents;

  for (uint32_t n = 0; n < numOps; n++)
  {
    Operand valueTypeOp = inst->GetInOperand(n*2);
    Instruction* valueTypeInst = context()->get_def_use_mgr()->GetDef(valueTypeOp.words[0]);

	if (valueTypeInst->opcode() == SpvOpUndef)
	  return false;
    
	valueTypeComponents.insert({n, {}});

	for (uint32_t i = 0; i < ColumnCount; ++i) {
      const uint32_t newVarID = context()->TakeNextId();

      std::vector<Operand> operands;
      operands.push_back({SPV_OPERAND_TYPE_ID, {valueTypeOp.words[0]}});
      operands.push_back({SPV_OPERAND_TYPE_LITERAL_INTEGER, {i}});

      Instruction* newVar = new Instruction(context(), SpvOpCompositeExtract, typeInst->GetOperand(1).words[0], newVarID, operands);
      valueTypeComponents[n].push_back(newVarID);

	  if (HasRelaxedPrecision(valueTypeOp.words[0])) {
        AddRelaxedPrecision(newVarID);
      }

      get_def_use_mgr()->AnalyzeInstDef(newVar);
      get_def_use_mgr()->AnalyzeInstUse(newVar);

      // If the variable is a constant, then insert at the beginning of the block
      if (!valueTypeInst->IsConstant()) {
		InsertAfterOpPhi(valueTypeInst, newVar);
      } else {
        newVar->InsertBefore(entryPoint);
      }
    }
  }

  // Create Op Phi instructions for new vectors
  std::vector<uint32_t> opPhiInstructs;
  for (uint32_t i = 0; i < ColumnCount; ++i) {
    const uint32_t newVarID = context()->TakeNextId();

    std::vector<Operand> operands;

	for (uint32_t n = 0; n < numOps; n++) {
      operands.push_back({SPV_OPERAND_TYPE_ID, {valueTypeComponents[n][i]}});
      operands.push_back({SPV_OPERAND_TYPE_ID, {inst->GetInOperand(n*2 + 1).words[0]}});
	}

    Instruction* newVar = new Instruction(context(), SpvOpPhi, typeInst->GetOperand(1).words[0], newVarID, operands);
    
	if (HasRelaxedPrecision(inst->GetInOperand(1).words[0]) && HasRelaxedPrecision(inst->GetInOperand(3).words[0])) {
      AddRelaxedPrecision(newVarID);
    }

	opPhiInstructs.push_back(newVarID);

    get_def_use_mgr()->AnalyzeInstDef(newVar);
    get_def_use_mgr()->AnalyzeInstUse(newVar);
    newVar->InsertBefore(inst);
  }

  // Create CompositeConstruct
  const uint32_t compositeVarID = context()->TakeNextId();

  std::vector<Operand> operands;
  for (uint32_t i = 0; i < opPhiInstructs.size(); ++i) {
	operands.push_back({SPV_OPERAND_TYPE_ID, {opPhiInstructs[i]}});
  }

  Instruction* compositeConstruction = new Instruction(context(), SpvOpCompositeConstruct, typeOp.words[0], compositeVarID, operands);
  get_def_use_mgr()->AnalyzeInstDef(compositeConstruction);
  get_def_use_mgr()->AnalyzeInstUse(compositeConstruction);

  if (HasRelaxedPrecision(HasRelaxedPrecision(inst->GetOperand(0).words[0]))) {
    AddRelaxedPrecision(compositeVarID);
  }

  InsertAfterOpPhi(inst, compositeConstruction);
  context()->ReplaceAllUsesWith(inst->result_id(), compositeVarID);
  inst->RemoveFromList();
  
  return true;
}

bool AndroidDriverPatchPass::FixupOpVectorShuffle(Instruction* inst) {
  if (inst->opcode() != SpvOpVectorShuffle) {
    return false;
  }

  // Only care about Matrix types
  const Operand inputOp1 = inst->GetInOperand(0);
  const Operand inputOp2 = inst->GetInOperand(1);

  const Instruction* inputInst1 = context()->get_def_use_mgr()->GetDef(inputOp1.words[0]);
  const Instruction* inputInst2 = context()->get_def_use_mgr()->GetDef(inputOp2.words[0]);

  // OpUnDef is supported in vectorshuffle but not in compositeextract, we could assign literals in this case
  if (inputInst1->opcode() == SpvOpUndef || inputInst2->opcode() == SpvOpUndef) {
    return false;
  }

  const Instruction* outputType = get_def_use_mgr()->GetDef(inst->type_id());
  const Instruction* inputType1 = get_def_use_mgr()->GetDef(inputInst1->type_id());
  const Instruction* inputType2 = get_def_use_mgr()->GetDef(inputInst2->type_id());

  bool bValidOutput = outputType->opcode() == SpvOpTypeVector && outputType->GetSingleWordOperand(2) == 3;
  bool bValidInput1 = (inputType1->opcode() == SpvOpTypeVector && inputType1->GetSingleWordOperand(2) == 4);
  bool bValidInput2 = (inputType2->opcode() == SpvOpTypeVector && inputType2->GetSingleWordOperand(2) == 4);

  if (!bValidOutput || (!bValidInput1 && !bValidInput2)) {
    return false;
  }

  bool bOutputHasRelaxedPrecision = HasRelaxedPrecision(inst->GetOperand(0).words[0]);

  uint32_t inputCompCount1 = inputType1->GetSingleWordOperand(2);

  // Gather indicies
  std::vector<uint32_t> indicies;
  for (uint32_t i = 2; i < inst->NumInOperands(); ++i) {
    const Operand op = inst->GetInOperand(i);

	// If we have read past the indexes then break
    if (op.type != SPV_OPERAND_TYPE_LITERAL_INTEGER) {
      break;
    }

	indicies.push_back(op.words[0]);
  }

  // Generate the composite extract op for each index
  std::vector<uint32_t> outputVariables;
  for (uint32_t index : indicies) {
    bool bUseVector2 = index >= inputCompCount1;
    const Instruction* inVector = bUseVector2 ? inputInst2 : inputInst1;
    uint32_t finalIndex = bUseVector2 ? index - inputCompCount1 : index;

	if (inVector->opcode() == SpvOpUndef) {
      const analysis::Constant* constFloat = context()->get_constant_mgr()->GetFloatConst(0.0);
	  outputVariables.push_back(constFloat->AsFloatConstant()->words()[0]);
	} else {
	  const uint32_t compositeExtractID = context()->TakeNextId();

      std::vector<Operand> operands;
      operands.push_back({SPV_OPERAND_TYPE_ID, {inVector->GetOperand(1).words[0]}});
      operands.push_back({SPV_OPERAND_TYPE_LITERAL_INTEGER, {finalIndex}});

      Instruction* compositeExtractVar =
        new Instruction(context(), SpvOpCompositeExtract,
                                          inputType1->GetOperand(1).words[0],
                                          compositeExtractID, operands);

	  outputVariables.push_back(compositeExtractID);

	  get_def_use_mgr()->AnalyzeInstDef(compositeExtractVar);
      compositeExtractVar->InsertBefore(inst);

      if (HasRelaxedPrecision(inVector->GetOperand(1).words[0])) {
        AddRelaxedPrecision(compositeExtractID);
      }
	}
  }

  // Create the composite construct
  const uint32_t compositeVarID = context()->TakeNextId();
  std::vector<Operand> operands;

  for (uint32_t outVar : outputVariables) {
    operands.push_back({SPV_OPERAND_TYPE_ID, {outVar}});
  }

  Instruction* compositeConstruction = new Instruction(context(), SpvOpCompositeConstruct, inst->GetOperand(0).words[0], compositeVarID, operands);

  if (bOutputHasRelaxedPrecision) {
    AddRelaxedPrecision(compositeVarID);
  }

  get_def_use_mgr()->AnalyzeInstDef(compositeConstruction);
  compositeConstruction->InsertBefore(inst);

  context()->ReplaceAllUsesWith(inst->result_id(), compositeVarID);
  inst->RemoveFromList();

  return true;
}

bool AndroidDriverPatchPass::FixupOpVariableFunctionPrecision(Instruction* inst) {
  if (inst->opcode() != SpvOpVariable) {
    return false;
  }
 
  // Check the storage class for this variable is a function
  uint32_t storageClass = inst->GetOperand(2).words[0];
  if (storageClass != SpvStorageClassFunction) {
    return false;
  }
	
  std::vector<Instruction*> decorations = get_decoration_mgr()->GetDecorationsFor(inst->GetOperand(0).words[0], false);
  for (Instruction* decoration : decorations) {
    if (decoration->GetSingleWordInOperand(1) == SpvDecorationRelaxedPrecision) {
      decoration->RemoveFromList();
      return true;
    }
  }

  return false;
}

bool AndroidDriverPatchPass::FixupOpTypeImage(Instruction* inst) {
  if (inst->opcode() != SpvOpTypeImage) {
    return false;
  }
  
  // If input dim is not subpass return
  uint32_t dimParam = inst->GetInOperand(1).words[0];
  if (dimParam != SpvDimSubpassData) {
    return false;
  }
	
  // If depth param != 2 (Unknown) then ignore this param
  uint32_t depthParam = inst->GetInOperand(2).words[0];
  if (depthParam != 2) {
    return false;
  }

  inst->SetInOperand(2, {0});

  return true;
}

bool AndroidDriverPatchPass::HasRelaxedPrecision(uint32_t operand_id) {
  std::vector<Instruction*> decorations =
      get_decoration_mgr()->GetDecorationsFor(operand_id, false);
  for (Instruction* decoration : decorations) {
    if (decoration->GetSingleWordInOperand(1) ==
        SpvDecorationRelaxedPrecision) {
      return true;
    }
  }
  return false;
}

void AndroidDriverPatchPass::AddRelaxedPrecision(uint32_t operand_id) {
  if (HasRelaxedPrecision(operand_id)) {
    return;
  }

  get_decoration_mgr()->AddDecoration(operand_id, SpvDecorationRelaxedPrecision);

  return;
}

bool AndroidDriverPatchPass::RemoveRelaxedPrecision(uint32_t operand_id) {
  std::vector<Instruction*> decorations = get_decoration_mgr()->GetDecorationsFor(operand_id, false);

  for (Instruction* decoration : decorations) {
    if (decoration->GetSingleWordInOperand(1) == SpvDecorationRelaxedPrecision) {
      get_decoration_mgr()->RemoveDecoration(decoration);
      return true;
    }
  }

  return false;
}


}  // namespace opt
}  // namespace spvtools
