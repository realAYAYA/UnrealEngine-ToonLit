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

#include "convert_composite_to_op_access_chain.h"

#include <set>

#include "source/opt/instruction.h"
#include "source/opt/ir_context.h"

namespace spvtools {
namespace opt {

Pass::Status ConvertCompositeToOpAccessChainPass::Process() {

	std::vector<Instruction*> InstructionsToDelete;
	// Find all array variables
	bool bHasChanged = false;
	context()->module()->ForEachInst([ &bHasChanged, &InstructionsToDelete, this](Instruction* inst) {
		if (inst->opcode() == spv::Op::OpCompositeConstruct) {
			const Instruction* ResultType_CompositeConstruct = context()->get_def_use_mgr()->GetDef(inst->GetOperand(0).words[0]);
			uint32_t ResultId_CompositeConstruct = inst->result_id();
			uint32_t OperandDataForArrayType = ResultType_CompositeConstruct->GetOperand(1).words[0];
			uint32_t resultId_PointerType = 0;
			std::vector<uint32_t> Operands_CompositeConstruct;

			// Grab operands from ComposireConstruct
			bool bAllOperandsAreConstant = true;
			for (uint32_t i = 2; i < inst->NumOperands(); ++i) {
				const Instruction* ConstIdx = context()->get_def_use_mgr()->GetDef(inst->GetOperand(i).words[0]);
				if (ConstIdx->opcode() != spv::Op::OpConstant) {
					bAllOperandsAreConstant = false;
				}
				Operands_CompositeConstruct.push_back(inst->GetOperand(i).words[0]);
			}

			if (ResultType_CompositeConstruct->opcode() == spv::Op::OpTypeArray && !bAllOperandsAreConstant) {

				//Find OpStores of this OpComposite
				std::vector<uint32_t> resultId_OpVariables;
				std::vector<uint32_t> storageClass_OpVariables;
				context()->module()->ForEachInst([&ResultId_CompositeConstruct, &resultId_OpVariables, &InstructionsToDelete, &resultId_PointerType, &OperandDataForArrayType, this](Instruction* inst) {
					if (inst->opcode() == spv::Op::OpStore) {
						if (inst->GetOperand(1).words[0] == ResultId_CompositeConstruct)
						{
							InstructionsToDelete.push_back(inst);
						
							resultId_OpVariables.push_back(inst->GetOperand(0).words[0]);
							// make sure the pointer type exists with element type and storage class same as the array
							if (resultId_PointerType == 0)
							{
								const Instruction* inst_OpVariables = context()->get_def_use_mgr()->GetDef(inst->GetOperand(0).words[0]);
								resultId_PointerType = context()->get_type_mgr()->FindPointerToType(OperandDataForArrayType, static_cast<spv::StorageClass>(inst_OpVariables->GetOperand(2).words[0]));
							}
						}
					}
				});

				assert((resultId_PointerType != 0) && "We couldn't create a pointer type for the OpAccessChain");

				// Add OpAccessChain + OpStore
				for (uint32_t resultId_OpVariable : resultId_OpVariables)
				{
					uint32_t index = 0;
					for (uint32_t Operands : Operands_CompositeConstruct) {
						uint32_t NewResultId = context()->TakeNextId();

						analysis::Integer unsigned_int_type(32, false);
						analysis::Type* registered_unsigned_int_type = context()->get_type_mgr()->GetRegisteredType(&unsigned_int_type);
						const analysis::Constant* NewConstant = context()->get_constant_mgr()->GetConstant(registered_unsigned_int_type, { index++ });
						Instruction* newConstInst = context()->get_constant_mgr()->GetDefiningInstruction(NewConstant);

						std::vector<Operand> operandsOpAccessChain;
						operandsOpAccessChain.push_back({ SPV_OPERAND_TYPE_ID, {resultId_OpVariable} });
						operandsOpAccessChain.push_back({ SPV_OPERAND_TYPE_ID, {newConstInst->GetOperand(1).words[0]} });
						Instruction* NewOpAccessChain = new Instruction(context(), spv::Op::OpAccessChain, resultId_PointerType, NewResultId, operandsOpAccessChain);
						NewOpAccessChain->InsertAfter(inst);

						std::vector<Operand> operandsOpStore;
						operandsOpStore.push_back({ SPV_OPERAND_TYPE_ID, {NewResultId} });
						operandsOpStore.push_back({ SPV_OPERAND_TYPE_ID, {Operands} });
						Instruction* NewOpStore = new Instruction(context(), spv::Op::OpStore, 0, 0, operandsOpStore);
						NewOpStore->InsertAfter(NewOpAccessChain);
					}
				}
				// Remove old OpCompositeConstruct and OpStore
				InstructionsToDelete.push_back(inst);

				bHasChanged = true;
			}
		}
	});

	for (Instruction* Inst : InstructionsToDelete)
	{
		context()->KillInst(Inst);
	}

  return bHasChanged ? Status::SuccessWithChange: Status::SuccessWithoutChange;
}

}  // namespace opt
}  // namespace spvtools
