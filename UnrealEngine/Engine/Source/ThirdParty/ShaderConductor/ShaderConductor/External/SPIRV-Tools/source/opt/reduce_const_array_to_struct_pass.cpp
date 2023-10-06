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
  
  // Find all array variables
  context()->module()->ForEachInst([&ArrayInst, this](Instruction* inst) {

    if (inst->opcode() == spv::Op::OpTypeArray) {

      bool bValid = false;
	  // Check that the array variables are 16 bytes wide
	  // TODO: Check Type
      context()->get_def_use_mgr()->ForEachUser(inst, [&bValid](Instruction* user) {
        if (user->opcode() == spv::Op::OpDecorate) {
          if (user->GetOperand(1).words[0] == (uint32_t)spv::Decoration::ArrayStride && user->GetOperand(2).words[0] == 16) {
            bValid = true;
          }
        }
      });

	  if(bValid) {
        ArrayInst.push_back(inst);
      }
    }
  });

  std::vector<ArrayStruct> ArrayStructs;

  // Look for structs which use the array type
  for (auto arrayType : ArrayInst) {
    context()->get_def_use_mgr()->ForEachUser(arrayType, [&ArrayStructs, &arrayType](Instruction* user) {
	  
      if (user->opcode() == spv::Op::OpTypeStruct) {
        for (uint32_t idx = 1; idx < user->NumOperands(); idx++) {
	      if (user->GetOperand(idx).words[0] == arrayType->GetOperand(0).words[0]) {
            ArrayStruct NewStruct;
            NewStruct.Array = arrayType;
            NewStruct.Struct = user;
            NewStruct.MemberIdx = idx - 1;
            ArrayStructs.push_back(NewStruct);
	      }
        }
      }
    });
  }

  // Sort by member index
  std::sort(ArrayStructs.begin(), ArrayStructs.end(), [](const ArrayStruct& a, const ArrayStruct& b) -> bool {
    return a.MemberIdx > b.MemberIdx;
  });

  for (auto & arrayStruct : ArrayStructs) {
    Instruction* NewStruct = nullptr;
    Instruction* OldStruct = arrayStruct.Struct;
    modified |= ReduceArray(arrayStruct, NewStruct);

	if (NewStruct) {
      for (auto& NextStruct : ArrayStructs) {
        if (NextStruct.Struct == OldStruct) {
          NextStruct.Struct = NewStruct;
        }
      }
    }
  }

  return modified ? Status::SuccessWithChange : Status::SuccessWithoutChange;
}

bool ReduceConstArrayToStructPass::ReduceArray(ArrayStruct& arrayStruct, Instruction *& NewStruct) {
  
  Instruction* arrayType = context()->get_def_use_mgr()->GetDef(arrayStruct.Array->GetOperand(0).words[0]);
  Instruction* structType = arrayStruct.Struct;
  uint32_t memberIdx = arrayStruct.MemberIdx;
  bool bIsGlobal = false;

  // We ignore global structures 
  context()->get_def_use_mgr()->ForEachUser(structType, [&bIsGlobal](Instruction* user) {
	if (user->opcode() == spv::Op::OpName) {
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

  struct MemberInfo
  {
    std::vector<Instruction*> MemberDecorations;
    Instruction* MemberDecorationOffset;
    Instruction* MemberName;
  };

  std::map<uint32_t, MemberInfo> memberInfoMap;

  // Find the instructions related to the structure
  context()->get_def_use_mgr()->ForEachUser(structType, [&pointerType, &memberInfoMap, &structType](Instruction* user) {
    if (user->opcode() == spv::Op::OpTypePointer) {
      if(user->GetOperand(2).words[0] == structType->GetOperand(0).words[0]) {
        pointerType = user;
	  }
    } else if (user->opcode() == spv::Op::OpMemberDecorate) {
      if (user->GetOperand(0).words[0] == structType->GetOperand(0).words[0]) {
        uint32_t structIdx = user->GetOperand(1).words[0];
        if (memberInfoMap.find(structIdx) == memberInfoMap.end()) {
          memberInfoMap.insert({structIdx, {std::vector<Instruction*>(), nullptr, nullptr}});
        }
        if (user->GetOperand(2).words[0] == (uint32_t)spv::Decoration::Offset) {
          memberInfoMap[structIdx].MemberDecorationOffset = user; 
        } else {
          memberInfoMap[structIdx].MemberDecorations.push_back(user); 
        }
        
      }
    } else if (user->opcode() == spv::Op::OpMemberName) {
      if (user->GetOperand(0).words[0] == structType->GetOperand(0).words[0]) {
        uint32_t structIdx = user->GetOperand(1).words[0];
        if (memberInfoMap.find(structIdx) == memberInfoMap.end()) {
          memberInfoMap.insert({structIdx, {std::vector<Instruction*>(), nullptr, nullptr}});
        }
        memberInfoMap[structIdx].MemberName = user; 
      }
    } 
  });

  if (pointerType == nullptr) {
    return false;
  }

  // We don't expect any additional decorations than offset for array
  if (memberInfoMap[memberIdx].MemberDecorations.size() > 0) {
    return false;
  }

  memberDecorateType = memberInfoMap[memberIdx].MemberDecorationOffset;
  memberNameType = memberInfoMap[memberIdx].MemberName;

  Instruction* variableType = nullptr;
  context()->get_def_use_mgr()->ForEachUser(
    pointerType, [&variableType, &pointerType](Instruction* user) {
    if (user->opcode() == spv::Op::OpVariable) {
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

  uint32_t structOffset = memberDecorateType->GetOperand(3).words[0];

  // Check for const access and that usage of variable is only OpAccessChain
  context()->get_def_use_mgr()->ForEachUser(
    variableType, [&variableType, &accessChains, &bInvalid, &structOffset, &memberIdx, this](Instruction* user) {
    if (user->opcode() == spv::Op::OpAccessChain) {
	  bool bMatchesStruct = user->GetOperand(2).words[0] == variableType->GetOperand(1).words[0];
	  
      if (bMatchesStruct) {
		if (user->NumOperands() < 4) {
          bInvalid = true;
        } else {
		  const Instruction* ConstIdx = context()->get_def_use_mgr()->GetDef(user->GetOperand(3).words[0]);
          if (ConstIdx->opcode() != spv::Op::OpConstant) {
		    bInvalid = true;
          } else {
            bool bMatchesArray = ConstIdx->GetOperand(2).words[0] == memberIdx;
            if (bMatchesArray) {
			  // Can't convert because access cannot be guarenteed as const
              if (user->NumOperands() < 5) {              
                bInvalid = true;
              } else {
				Operand constOperand = user->GetOperand(4);
              	const Instruction* ConstInst = context()->get_def_use_mgr()->GetDef(constOperand.words[0]);
              	if (ConstInst->opcode() != spv::Op::OpConstant) {
                  bInvalid = true;
                } else {
                  uint32_t ConstVal = ConstInst->GetOperand(2).words[0];
                  accessChains.push_back({ConstVal, (ConstVal * 4 * 4) + structOffset, user});
				}
              }
			}
          }
		}
      }
    } else if (user->opcode() == spv::Op::OpInBoundsAccessChain ||
               user->opcode() == spv::Op::OpPtrAccessChain) {
      bInvalid = true;
	}

  });

  // Sort elements into offset order
  std::sort(accessChains.begin(), accessChains.end(), [](const AccessChainData& a, const AccessChainData& b) -> bool {
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
  uint32_t numConstants = 0;

  for (auto & AccessChainData : accessChains) {

	// Create the OpMemberName instructions
	if (uniqueKeys.find(AccessChainData.constantValue) == uniqueKeys.end()) {
      {
        std::vector<Operand> operands;
        operands.push_back({SPV_OPERAND_TYPE_ID, {structType->GetOperand(0).words[0]}});
        operands.push_back({SPV_OPERAND_TYPE_LITERAL_INTEGER, {n + memberIdx}});

        std::string memberName = structName + "_" + std::to_string(AccessChainData.constantValue);
        auto memberNameVector = utils::MakeVector(memberName);
        operands.push_back({SPV_OPERAND_TYPE_LITERAL_STRING, std::move(memberNameVector)});

        Instruction* NewVar = new Instruction(context(), spv::Op::OpMemberName, 0, 0, operands);
        newOpMemberNames.push_back(NewVar);
      }

	  // Create the OpMemberDecorate instructions
      {
        std::vector<Operand> operands;
        operands.push_back({SPV_OPERAND_TYPE_ID, {structType->GetOperand(0).words[0]}});
        operands.push_back({SPV_OPERAND_TYPE_LITERAL_INTEGER, {n + memberIdx}});
        operands.push_back({SPV_OPERAND_TYPE_DECORATION, {uint32_t(spv::Decoration::Offset)}});
        operands.push_back({SPV_OPERAND_TYPE_LITERAL_INTEGER, {AccessChainData.offset}});

        Instruction* NewVar = new Instruction(context(), spv::Op::OpMemberDecorate, 0, 0, operands);
        newOpMemberDecorates.push_back(NewVar);
      }
      uniqueKeys.insert({AccessChainData.constantValue, n});

	  n++;
    }

	// Create the new Accesses to the struct 
	{
	  analysis::Integer unsigned_int_type(32, false);
	  analysis::Type* registered_unsigned_int_type = context()->get_type_mgr()->GetRegisteredType(&unsigned_int_type);
	  const analysis::Constant* newConstant = context()->get_constant_mgr()->GetConstant(registered_unsigned_int_type, {uniqueKeys[AccessChainData.constantValue]+memberIdx});
      Instruction* constInst = context()->get_constant_mgr()->GetDefiningInstruction(newConstant);

      std::vector<Operand> operands;
      
      operands.push_back({SPV_OPERAND_TYPE_ID, {AccessChainData.accessChain->GetOperand(2).words[0]}});
      operands.push_back({SPV_OPERAND_TYPE_ID, {constInst->result_id()}});
      if(AccessChainData.accessChain->NumOperands() > 5) {
        operands.push_back({SPV_OPERAND_TYPE_ID, {AccessChainData.accessChain->GetOperand(5).words[0]}});
	  }

	  Instruction* newVar = new Instruction(context(), spv::Op::OpAccessChain, AccessChainData.accessChain->GetOperand(0).words[0], AccessChainData.accessChain->result_id(), operands);
      newVar->InsertBefore(AccessChainData.accessChain);

      AccessChainData.accessChain->RemoveFromList();
	}
  }

  numConstants = n;

  // Offset existing struct members to new indicies
  for (auto& infoPair : memberInfoMap) {
    if (infoPair.first > memberIdx) {
      {
        Operand& opIdx = infoPair.second.MemberDecorationOffset->GetOperand(1);
        opIdx.words[0] = opIdx.words[0] + numConstants-1;
      }

	  for (Instruction* memberDecorateInst : infoPair.second.MemberDecorations) {
		Operand& opIdx = memberDecorateInst->GetOperand(1);
		opIdx.words[0] = opIdx.words[0] + numConstants-1;
	  }

	  {
        Operand& opIdx = infoPair.second.MemberName->GetOperand(1);
        opIdx.words[0] = opIdx.words[0] + numConstants-1;
      }
    }
  }

  // Find SpvOpAccessChains using struct and offset
  context()->get_def_use_mgr()->ForEachUser(
    variableType, [&variableType, &memberIdx, &numConstants, this](Instruction* user) {
    if (user->opcode() == spv::Op::OpAccessChain) {
	  bool bMatchesStruct = user->GetOperand(2).words[0] == variableType->GetOperand(1).words[0];
	  
      if (bMatchesStruct) {
		const Instruction* constIdx = context()->get_def_use_mgr()->GetDef(user->GetOperand(3).words[0]);
        uint32_t memberAccessIdx = constIdx->GetOperand(2).words[0];
        if (memberAccessIdx > memberIdx) {
          Operand constOperand = user->GetOperand(3);

          const Instruction* constInst = context()->get_def_use_mgr()->GetDef(constOperand.words[0]);
          uint32_t constVal = constInst->GetOperand(2).words[0];

		  analysis::Integer unsigned_int_type(32, false);
	      analysis::Type* registered_unsigned_int_type = context()->get_type_mgr()->GetRegisteredType(&unsigned_int_type);
	      const analysis::Constant* NewConstant = context()->get_constant_mgr()->GetConstant(registered_unsigned_int_type, {constVal + numConstants-1});
          Instruction* newConstInst = context()->get_constant_mgr()->GetDefiningInstruction(NewConstant);

		  user->GetOperand(3).words[0] = newConstInst->result_id();
		}
      }
    }
  });

  for (Instruction* newMemberName : newOpMemberNames) {
    newMemberName->InsertBefore(memberNameType);
  }
  memberNameType->RemoveFromList();

  for (Instruction* newMemberDecorate : newOpMemberDecorates) {
    newMemberDecorate->InsertBefore(memberDecorateType);
  }
  memberDecorateType->RemoveFromList();

  {
    std::vector<Operand> operands;

	// Insert types before the new ones
    for (uint32_t i = 0; i < memberIdx; ++i) {
      operands.push_back(structType->GetOperand(i + 1));
	}

	// Insert new types
    for (uint32_t i = 0; i < uniqueKeys.size(); ++i) {
      operands.push_back(arrayType->GetOperand(1));
    }

	// Insert types after the new ones
	for (uint32_t i = memberIdx + 1; i < memberInfoMap.size(); ++i) {
      operands.push_back(structType->GetOperand(i + 1));
    }

	// Create the new struct
	Instruction* newTypeStructVar = new Instruction(context(), spv::Op::OpTypeStruct, 0, structType->GetOperand(0).words[0], operands);
	newTypeStructVar->InsertBefore(structType);
    structType->RemoveFromList();

    pointerType->GetOperand(2).words[0] = newTypeStructVar->GetOperand(0).words[0];

	context()->InvalidateAnalysesExceptFor(opt::IRContext::kAnalysisNone);
    
	NewStruct = newTypeStructVar;
  }

  return true;
}

}  // namespace opt
}  // namespace spvtools
