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

#include "reduce_const_array_to_struct_pass.h"

#include <set>

#include "source/opt/instruction.h"
#include "source/opt/ir_context.h"

namespace spvtools {
namespace opt {

Pass::Status ReduceConstArrayToStructPass::Process() {
  bool modified = false;
  std::vector<Instruction*> ArrayInst;
  
  context()->module()->ForEachInst([&ArrayInst](Instruction* inst) {
    if (inst->opcode() == SpvOpTypeArray) {
      ArrayInst.push_back(inst);    
    }
  });

  for (Instruction* inst : ArrayInst) {
    modified |= ReduceArray(inst);
  }

  return modified ? Status::SuccessWithChange : Status::SuccessWithoutChange;
}

bool ReduceConstArrayToStructPass::ReduceArray(Instruction* inst) {
  
  Instruction* arrayType = context()->get_def_use_mgr()->GetDef(inst->GetOperand(0).words[0]);
  Instruction* structType = nullptr;
  Instruction* decorateType = nullptr;

  // Look for structs which use the array type
  context()->get_def_use_mgr()->ForEachUser(arrayType, [&structType, &decorateType, &arrayType](Instruction* user) {
    if (user->opcode() == SpvOpTypeStruct) {
	  // Only consider structs that contains a single array
      if(user->GetOperand(1).words[0] == arrayType->GetOperand(0).words[0] && user->NumOperands() == 2) {
        structType = user;
	  }
    }

	if (user->opcode() == SpvOpDecorate) {
      if (user->GetOperand(1).words[0] == SpvDecorationArrayStride && user->GetOperand(2).words[0] == 16) {
        decorateType = user;
      }
    }
  });

  if (structType == nullptr || decorateType == nullptr)
	return false;

  bool bIsGlobal = false;

  // We ignore global structures 
  context()->get_def_use_mgr()->ForEachUser(structType, [&bIsGlobal](Instruction* user) {
	if (user->opcode() == SpvOpName) {
      if (!user->GetOperand(1).AsString().compare("type.$Globals")) {
        bIsGlobal = true;
      }
    } 
  });

  if (bIsGlobal) {
    return false;
  }

  Instruction* pointerType = nullptr;
  Instruction* memberDecorateType = nullptr;
  Instruction* memberNameType = nullptr;

  // Find the instructions related to the structure
  context()->get_def_use_mgr()->ForEachUser(structType, [&pointerType, &memberDecorateType, &memberNameType, &structType](Instruction* user) {
    if (user->opcode() == SpvOpTypePointer) {
      if(user->GetOperand(2).words[0] == structType->GetOperand(0).words[0]) {
        pointerType = user;
	  }
    }
    else if (user->opcode() == SpvOpMemberDecorate) {
      if (user->GetOperand(0).words[0] == structType->GetOperand(0).words[0]) {
       memberDecorateType = user;
      }
    } 
	else if (user->opcode() == SpvOpMemberName) {
      if (user->GetOperand(0).words[0] == structType->GetOperand(0).words[0]) {
        memberNameType = user;
      }
    } 
  });

  if (pointerType == nullptr) {
    return false;
  }

  Instruction* variableType = nullptr;
  context()->get_def_use_mgr()->ForEachUser(
    pointerType, [&variableType, &pointerType](Instruction* user) {
    if (user->opcode() == SpvOpVariable) {
      if (user->GetOperand(0).words[0] == pointerType->GetOperand(0).words[0]) {
        variableType = user;
      }
    }
  });

  if (variableType == nullptr) {
    return false;
  }

  struct AccessChainData {
    uint32_t constantValue;
    uint32_t offset;
    Instruction* accessChain;
  };

  std::vector<AccessChainData> accessChains;
  bool bInvalid = false;

  // Check for const access and that usage of variable is only SpvOpAccessChain
  context()->get_def_use_mgr()->ForEachUser(
    variableType, [&variableType, &accessChains, &bInvalid, this](Instruction* user) {
    if (user->opcode() == SpvOpAccessChain) {
      if (user->GetOperand(2).words[0] == variableType->GetOperand(1).words[0]) {
		if (user->NumOperands() < 5) {
          bInvalid = true;
        } else {
          Operand constOperand = user->GetOperand(4);
          const Instruction* ConstInst = context()->get_def_use_mgr()->GetDef(constOperand.words[0]);
          if (ConstInst->opcode() != SpvOpConstant) {
            bInvalid = true;
          } else {
            uint32_t ConstVal = ConstInst->GetOperand(2).words[0];
            accessChains.push_back({ConstVal, ConstVal * 4 * 4, user});
          }
		}
      }
    } else if (user->opcode() == SpvOpInBoundsAccessChain || user->opcode() == SpvOpPtrAccessChain) {
      bInvalid = true;
	}

  });

  std::sort(accessChains.begin(), accessChains.end(), 
	  [](const AccessChainData& a, const AccessChainData& b) -> bool {
        return a.offset < b.offset;
      });

  if (bInvalid) {
    return false;
  }

  std::vector<Instruction*> newOpMemberNames;
  std::vector<Instruction*> newOpMemberDecorates;
  std::string structName = memberNameType->GetOperand(2).AsString();
  std::map<uint32_t, uint32_t> uniqueKeys; 

  uint32_t n = 0;
  for (auto & AccessChainData : accessChains) {

	// Create the SpvOpMemberName instructions
	if (uniqueKeys.find(AccessChainData.constantValue) == uniqueKeys.end()) {
      {
        std::vector<Operand> operands;
        operands.push_back({SPV_OPERAND_TYPE_ID, {structType->GetOperand(0).words[0]}});
        operands.push_back({SPV_OPERAND_TYPE_LITERAL_INTEGER, {n}});

        std::string MemberName = structName + "_" + std::to_string(AccessChainData.constantValue);
        auto MemberNameVector = utils::MakeVector(MemberName);
        operands.push_back({SPV_OPERAND_TYPE_LITERAL_STRING, std::move(MemberNameVector)});

        Instruction* NewVar = new Instruction(context(), SpvOpMemberName, 0, 0, operands);
        newOpMemberNames.push_back(NewVar);
      }

	  // Create the SpvOpMemberDecorate instructions
      {
        std::vector<Operand> operands;
        operands.push_back({SPV_OPERAND_TYPE_ID, {structType->GetOperand(0).words[0]}});
        operands.push_back({SPV_OPERAND_TYPE_LITERAL_INTEGER, {n}});
        operands.push_back({SPV_OPERAND_TYPE_DECORATION, {SpvDecorationOffset}});
        operands.push_back({SPV_OPERAND_TYPE_LITERAL_INTEGER, {AccessChainData.offset}});

        Instruction* NewVar = new Instruction(context(), SpvOpMemberDecorate, 0, 0, operands);
        newOpMemberDecorates.push_back(NewVar);
      }
      uniqueKeys.insert({AccessChainData.constantValue, n});

	  n++;
    }

	// Create the new Accesses to the struct 
	{
	  analysis::Integer unsigned_int_type(32, false);
	  analysis::Type* registered_unsigned_int_type = context()->get_type_mgr()->GetRegisteredType(&unsigned_int_type);
	  const analysis::Constant* NewConstant = context()->get_constant_mgr()->GetConstant(registered_unsigned_int_type, {uniqueKeys[AccessChainData.constantValue]});
      Instruction* ConstInst = context()->get_constant_mgr()->GetDefiningInstruction(NewConstant);
      
	  get_def_use_mgr()->AnalyzeInstDef(ConstInst);
      get_def_use_mgr()->AnalyzeInstUse(ConstInst);

      std::vector<Operand> operands;
      
      operands.push_back({SPV_OPERAND_TYPE_ID, {AccessChainData.accessChain->GetOperand(2).words[0]}});
      operands.push_back({SPV_OPERAND_TYPE_ID, {ConstInst->result_id()}});
      if(AccessChainData.accessChain->NumOperands() > 5) {
        operands.push_back({SPV_OPERAND_TYPE_ID, {AccessChainData.accessChain->GetOperand(5).words[0]}});
	  }

	  Instruction* newVar = new Instruction(context(), SpvOpAccessChain, AccessChainData.accessChain->GetOperand(0).words[0], AccessChainData.accessChain->result_id(), operands);
      
	  get_def_use_mgr()->AnalyzeInstDef(newVar);
      get_def_use_mgr()->AnalyzeInstUse(newVar);

      newVar->InsertBefore(AccessChainData.accessChain);
      AccessChainData.accessChain->RemoveFromList();
	}
  }

  for (Instruction* newMemberName : newOpMemberNames) {
    get_def_use_mgr()->AnalyzeInstDef(newMemberName);
    get_def_use_mgr()->AnalyzeInstUse(newMemberName);
    newMemberName->InsertBefore(memberNameType);
  }
  memberNameType->RemoveFromList();

  for (Instruction* newMemberDecorate : newOpMemberDecorates) {
  
	get_def_use_mgr()->AnalyzeInstDef(newMemberDecorate);
    get_def_use_mgr()->AnalyzeInstUse(newMemberDecorate);
    newMemberDecorate->InsertBefore(memberDecorateType);
  }
  memberDecorateType->RemoveFromList();

  {
    std::vector<Operand> operands;
    for (uint32_t i = 0; i < uniqueKeys.size(); ++i) {
      operands.push_back(arrayType->GetOperand(1));
    }

	// Create the new struct
	Instruction* newTypeStructVar = new Instruction(context(), SpvOpTypeStruct, structType->GetOperand(0).words[0], 0, operands);

	get_def_use_mgr()->AnalyzeInstDef(newTypeStructVar);
    get_def_use_mgr()->AnalyzeInstUse(newTypeStructVar);

	newTypeStructVar->InsertBefore(structType);
    structType->RemoveFromList();
  }

  return true;
}

}  // namespace opt
}  // namespace spvtools
