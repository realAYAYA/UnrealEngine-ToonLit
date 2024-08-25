// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMUnitNode.h"

#include "RigVMCore/RigVMStruct.h"
#include "RigVMModel/RigVMControllerActions.h"
#include "RigVMUserWorkflowRegistry.h"
#include "RigVMHost.h"
#include "RigVMBlueprint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMUnitNode)

void URigVMUnitNode::PostLoad()
{
	Super::PostLoad();

	// if we have a script struct but no notation let's figure out the template
	if(GetScriptStruct() != nullptr)
	{
		if (IsOutDated())
		{
			TemplateNotation = NAME_None;
			if(const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(GetScriptStruct(), *GetMethodName().ToString()))
			{
				ResolvedFunctionName = Function->GetName();
			}
		}
		else if(GetTemplate() == nullptr)
		{
			if(const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(GetScriptStruct(), *GetMethodName().ToString()))
			{
				if(Function->TemplateIndex != INDEX_NONE)
				{
					const FRigVMTemplate& Template = FRigVMRegistry::Get().GetTemplates()[Function->TemplateIndex];
					TemplateNotation = Template.GetNotation();				
				}
				ResolvedFunctionName = Function->GetName();
			}			
		}
	}
}

FString URigVMUnitNode::GetNodeTitle() const
{
	if (UScriptStruct* Struct = GetScriptStruct())
	{
		return Struct->GetDisplayNameText().ToString();
	}
	return Super::GetNodeTitle();
}

FText URigVMUnitNode::GetToolTipText() const
{
	if (UScriptStruct* Struct = GetScriptStruct())
	{
		return Struct->GetToolTipText();
	}
	return URigVMNode::GetToolTipText();
}

bool URigVMUnitNode::IsDefinedAsConstant() const
{
	if (UScriptStruct* Struct = GetScriptStruct())
	{
		return Struct->HasMetaData(FRigVMStruct::ConstantMetaName);
	}
	return false;
}

bool URigVMUnitNode::IsDefinedAsVarying() const
{
	if (UScriptStruct* Struct = GetScriptStruct())
	{
		return Struct->HasMetaData(FRigVMStruct::VaryingMetaName);
	}
	return false;
}

FName URigVMUnitNode::GetEventName() const
{
	TSharedPtr<FStructOnScope> StructOnScope = ConstructStructInstance(false);
	if (StructOnScope.IsValid())
	{
		const FRigVMStruct* StructMemory = (FRigVMStruct*)StructOnScope->GetStructMemory();
		return StructMemory->GetEventName();
	}
	return Super::GetEventName();
}

bool URigVMUnitNode::CanOnlyExistOnce() const
{
	TSharedPtr<FStructOnScope> StructOnScope = ConstructStructInstance(false);
	if (StructOnScope.IsValid())
	{
		const FRigVMStruct* StructMemory = (FRigVMStruct*)StructOnScope->GetStructMemory();
		return StructMemory->CanOnlyExistOnce();
	}
	return Super::CanOnlyExistOnce();
}

const TArray<FName>& URigVMUnitNode::GetControlFlowBlocks() const
{
	const TSharedPtr<FStructOnScope> StructOnScope = ConstructStructInstance(true);
	if (StructOnScope.IsValid())
	{
		const FRigVMStruct* StructMemory = (FRigVMStruct*)StructOnScope->GetStructMemory();
		return StructMemory->GetControlFlowBlocks();
	}
	return Super::GetControlFlowBlocks();
}

const bool URigVMUnitNode::IsControlFlowBlockSliced(const FName& InBlockName) const
{
	const TSharedPtr<FStructOnScope> StructOnScope = ConstructStructInstance(true);
	if (StructOnScope.IsValid())
	{
		const FRigVMStruct* StructMemory = (FRigVMStruct*)StructOnScope->GetStructMemory();
		return StructMemory->IsControlFlowBlockSliced(InBlockName);
	}
	return Super::IsControlFlowBlockSliced(InBlockName);
}

FText URigVMUnitNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	if(UScriptStruct* Struct = GetScriptStruct())
	{
		TArray<FString> Parts;
		URigVMPin::SplitPinPath(InPin->GetPinPath(), Parts);

		for (int32 PartIndex = 1; PartIndex < Parts.Num(); PartIndex++)
		{
			FProperty* Property = Struct->FindPropertyByName(*Parts[PartIndex]);
			if (!Property)
			{
				break;
			}

			if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				if (PartIndex < Parts.Num() - 1)
				{
					Property = ArrayProperty->Inner;
					PartIndex++;
				}
			}

			if (PartIndex == Parts.Num() - 1)
			{
				return Property->GetToolTipText();
			}

			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				Struct = StructProperty->Struct;
			}
		}

	}
	return URigVMNode::GetToolTipTextForPin(InPin);
}

bool URigVMUnitNode::ShouldInputPinComputeLazily(const URigVMPin* InPin) const
{
	const URigVMPin* RootPin = InPin->GetRootPin();
	check(RootPin->GetNode() == this);

	if(const UScriptStruct* Struct = GetScriptStruct())
	{
		if(const FProperty* Property = Struct->FindPropertyByName(RootPin->GetFName()))
		{
			return Property->HasMetaData(FRigVMStruct::ComputeLazilyMetaName);
		}
	}

	return Super::ShouldInputPinComputeLazily(InPin);
}

bool URigVMUnitNode::IsOutDated() const
{
	return !GetDeprecatedMetadata().IsEmpty();
}

FString URigVMUnitNode::GetDeprecatedMetadata() const
{
	if (UScriptStruct* Struct = GetScriptStruct())
	{
		FString DeprecatedMetadata;
		if(Struct->GetStringMetaDataHierarchical(FRigVMStruct::DeprecatedMetaName, &DeprecatedMetadata))
		{
			return DeprecatedMetadata;
		}
	}
	return FString();
}

TArray<FRigVMUserWorkflow> URigVMUnitNode::GetSupportedWorkflows(ERigVMUserWorkflowType InType, const UObject* InSubject) const
{
	TArray<FRigVMUserWorkflow> Workflows = Super::GetSupportedWorkflows(InType, InSubject);

	if(InSubject == nullptr)
	{
		InSubject = this;
	}

	if(UScriptStruct* Struct = GetScriptStruct())
	{
		check(Struct->IsChildOf(FRigVMStruct::StaticStruct()));

		const TSharedPtr<FStructOnScope> StructOnScope = ConstructStructInstance();
		const FRigVMStruct* StructMemory = (const FRigVMStruct*)StructOnScope->GetStructMemory();
		TArray<FRigVMUserWorkflow> StructWorkflows = StructMemory->GetWorkflows(InType, InSubject);
		StructWorkflows.Append(URigVMUserWorkflowRegistry::Get()->GetWorkflows(InType, Struct, InSubject));
		Swap(Workflows, StructWorkflows);
		Workflows.Append(StructWorkflows);
	}

	return Workflows;
}

TArray<URigVMPin*> URigVMUnitNode::GetAggregateInputs() const
{
	TArray<URigVMPin*> AggregateInputs;
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	if (const UScriptStruct* Struct = GetScriptStruct())
	{
		for (URigVMPin* Pin : GetPins())
		{
			if (Pin->GetDirection() == ERigVMPinDirection::Input)
			{
				if (const FProperty* Property = Struct->FindPropertyByName(Pin->GetFName()))
				{
					if (Property->HasMetaData(FRigVMStruct::AggregateMetaName))
					{
						AggregateInputs.Add(Pin);
					}
				}			
			}
		}
	}
	else
	{
		return Super::GetAggregateInputs();
	}
#endif
	return AggregateInputs;
}

TArray<URigVMPin*> URigVMUnitNode::GetAggregateOutputs() const
{
	TArray<URigVMPin*> AggregateOutputs;
#if UE_RIGVM_AGGREGATE_NODES_ENABLED	
	if (const UScriptStruct* Struct = GetScriptStruct())
	{
		for (URigVMPin* Pin : GetPins())
		{
			if (Pin->GetDirection() == ERigVMPinDirection::Output)
			{
				if (const FProperty* Property = Struct->FindPropertyByName(Pin->GetFName()))
				{
					if (Property->HasMetaData(FRigVMStruct::AggregateMetaName))
					{
						AggregateOutputs.Add(Pin);
					}
				}			
			}
		}
	}
	else
	{
		return Super::GetAggregateOutputs();
	}
#endif
	return AggregateOutputs;
}

FName URigVMUnitNode::GetNextAggregateName(const FName& InLastAggregatePinName) const
{
#if UE_RIGVM_AGGREGATE_NODES_ENABLED	
	if(const UScriptStruct* Struct = GetScriptStruct())
	{
		check(Struct->IsChildOf(FRigVMStruct::StaticStruct()));

		const TSharedPtr<FStructOnScope> StructOnScope = ConstructStructInstance();
		const FRigVMStruct* StructMemory = (const FRigVMStruct*)StructOnScope->GetStructMemory();
		return StructMemory->GetNextAggregateName(InLastAggregatePinName);
	}

	return Super::GetNextAggregateName(InLastAggregatePinName);
#else
	return FName();
#endif
}

UScriptStruct* URigVMUnitNode::GetScriptStruct() const
{
	if(UScriptStruct* ResolvedStruct = Super::GetScriptStruct())
	{
		return ResolvedStruct;
	}
	return ScriptStruct_DEPRECATED;
}

FName URigVMUnitNode::GetMethodName() const
{
	const FName ResolvedMethodName = Super::GetMethodName();
	if(!ResolvedMethodName.IsNone())
	{
		return ResolvedMethodName;
	}
	return MethodName_DEPRECATED;
}

FString URigVMUnitNode::GetStructDefaultValue() const
{
	TArray<FString> PinDefaultValues;
	for (URigVMPin* Pin : GetPins())
	{
		FString PinDefaultValue = Pin->GetDefaultValue();
		if (Pin->IsStringType())
		{
			PinDefaultValue = TEXT("\"") + PinDefaultValue + TEXT("\"");
		}
		else if (PinDefaultValue.IsEmpty() || PinDefaultValue == TEXT("()"))
		{
			continue;
		}
		PinDefaultValues.Add(FString::Printf(TEXT("%s=%s"), *Pin->GetName(), *PinDefaultValue));
	}
	if (PinDefaultValues.Num() == 0)
	{
		return TEXT("()");
	}
	return FString::Printf(TEXT("(%s)"), *FString::Join(PinDefaultValues, TEXT(",")));

}

TSharedPtr<FStructOnScope> URigVMUnitNode::ConstructStructInstance(bool bUseDefault) const
{
	if (UScriptStruct* Struct = GetScriptStruct())
	{
		TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Struct));
		FRigVMStruct* StructMemory = (FRigVMStruct*)StructOnScope->GetStructMemory();
		if (!bUseDefault)
		{
			FString StructDefaultValue = GetStructDefaultValue();
			Struct->ImportText(*StructDefaultValue, StructMemory, nullptr, PPF_IncludeTransient, GLog, Struct->GetName());
		}
		return StructOnScope;
	}
	return nullptr;
}

void URigVMUnitNode::EnumeratePropertiesOnHostAndStructInstance(URigVMHost* InHost, TSharedPtr<FStructOnScope> InInstance, bool bPreferLiterals, TFunction<void(const URigVMPin*,const FProperty*,uint8*,const FProperty*,uint8*)> InEnumerationFunction, int32 InSliceIndex) const
{
	check(InHost);
	check(InInstance.IsValid() && InInstance->IsValid());

	const URigVMBlueprint* Blueprint = GetTypedOuter<URigVMBlueprint>();
	if(Blueprint == nullptr)
	{
		return;
	}

	URigVM* VM = InHost->GetVM();
	if(VM == nullptr)
	{
		return;
	}

	const FRigVMByteCode& ByteCode = VM->GetByteCode();
	const TArray<int32>& InstructionIndices = ByteCode.GetAllInstructionIndicesForSubject(const_cast<URigVMUnitNode*>(this));
	if(InstructionIndices.IsEmpty())
	{
		return;
	}

	const FRigVMInstructionArray& Instructions = ByteCode.GetInstructions();
	for(int32 Index = 0; Index < InstructionIndices.Num(); Index++)
	{
		const int32 InstructionIndex = InstructionIndices[Index]; 
		if(Instructions[InstructionIndices[Index]].OpCode != ERigVMOpCode::Execute)
		{
			continue;
		}

		const FRigVMInstruction& Instruction = Instructions[InstructionIndex];
		const FRigVMOperandArray Operands = ByteCode.GetOperandsForOp(Instruction);
		const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InInstance->GetStruct());

		const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(const_cast<UScriptStruct*>(ScriptStruct), *GetMethodName().ToString());
		if(Function == nullptr)
		{
			return;
		}

		for(int32 ArgumentIndex = 0; ArgumentIndex < Function->GetArguments().Num(); ArgumentIndex++)
		{
			const FRigVMFunctionArgument& Argument = Function->GetArguments()[ArgumentIndex];
			const URigVMPin* Pin = FindPin(Argument.Name);
			if(Pin == nullptr)
			{
				continue;
			}
			
			FRigVMOperand Operand = Operands[ArgumentIndex];

			if(bPreferLiterals && Operand.GetMemoryType() != ERigVMMemoryType::Literal)
			{
				// walk backwards on the bytecode to find the copy expression copying
				// the default value literal into the target operand during initialization
				for(int32 CopyInstructionIndex = InstructionIndex - 1; CopyInstructionIndex >= 0; CopyInstructionIndex--)
				{
					const FRigVMInstruction& CopyInstruction = Instructions[CopyInstructionIndex];
					if(CopyInstruction.OpCode == ERigVMOpCode::Copy)
					{
						const FRigVMCopyOp& CopyOp = ByteCode.GetOpAt<FRigVMCopyOp>(CopyInstruction);
						if(CopyOp.Target == Operand &&
							CopyOp.Source.GetMemoryType() == ERigVMMemoryType::Literal &&
							CopyOp.Source.GetRegisterOffset() == INDEX_NONE)
						{
							Operand = CopyOp.Source;
							break;
						}
					}
				}
			}

			const FProperty* InstanceProperty = ScriptStruct->FindPropertyByName(Argument.Name);
			if(InstanceProperty == nullptr)
			{
				continue;
			}

			uint8* InstanceMemory = InstanceProperty->ContainerPtrToValuePtr<uint8>(InInstance->GetStructMemory());
			uint8* HostMemory = nullptr;
			const FProperty* HostProperty = nullptr;
			
			switch(Operand.GetMemoryType())
			{
			case ERigVMMemoryType::External:
				{
					TArray<FRigVMExternalVariable> ExternalVariables = InHost->GetExternalVariables();
					if(ExternalVariables.IsValidIndex(Operand.GetRegisterIndex()))
					{
						const FRigVMExternalVariable& ExternalVariable = ExternalVariables[Operand.GetRegisterIndex()];
						HostMemory = ExternalVariable.Memory; 
						HostProperty = ExternalVariable.Property;
					}
					break;
				}
			case ERigVMMemoryType::Literal:
				{
					if(FRigVMMemoryStorageStruct* LiteralMemory = InHost->GetLiteralMemory())
					{
						if(const FProperty* Property = LiteralMemory->GetProperty(Operand.GetRegisterIndex()))
						{
							HostMemory = Property->ContainerPtrToValuePtr<uint8>(LiteralMemory->GetContainerPtr());
							HostProperty = Property;
						}
					}
					break;
				}
			case ERigVMMemoryType::Work:
				{
					if(FRigVMMemoryStorageStruct* WorkMemory = InHost->GetWorkMemory())
					{
						if(const FProperty* Property = WorkMemory->GetProperty(Operand.GetRegisterIndex()))
						{
							HostMemory = Property->ContainerPtrToValuePtr<uint8>(WorkMemory->GetContainerPtr());
							HostProperty = Property;
						}
					}
					break;
				}
			default:
				{
					break;
				}
			}

			// hidden properties are backed up by sliced memory
			if(Pin->GetDirection() == ERigVMPinDirection::Hidden)
			{
				if(InSliceIndex < 0)
				{
					InSliceIndex = 0;
				}
				
				if(const FArrayProperty* HostArrayProperty = CastField<FArrayProperty>(HostProperty))
				{
					FScriptArrayHelper ArrayHelper(HostArrayProperty, HostMemory);
					if(ArrayHelper.Num() <= InSliceIndex)
					{
						UE_LOG(LogRigVMDeveloper, Warning,
							TEXT("Provided slice index %d is invalid when enumerating properties on node '%s' (total number of slices is %d)."),
							InSliceIndex, *GetPathName(), ArrayHelper.Num());
						continue;
					}

					HostProperty = HostArrayProperty->Inner;
					HostMemory = ArrayHelper.GetRawPtr(InSliceIndex);
				}
			}

			if(HostMemory && HostProperty && InstanceProperty && InstanceMemory)
			{
				InEnumerationFunction(Pin, HostProperty, HostMemory, InstanceProperty, InstanceMemory);
			}
		}
	}
}

TSharedPtr<FStructOnScope> URigVMUnitNode::ConstructLiveStructInstance(URigVMHost* InHost, int32 InSliceIndex) const
{
	check(InHost);
	
	TSharedPtr<FStructOnScope> NodeInstance = ConstructStructInstance(false);
	if(!NodeInstance.IsValid() || !NodeInstance->IsValid())
	{
		return NodeInstance;
	}

	EnumeratePropertiesOnHostAndStructInstance(InHost, NodeInstance, false,
	[](const URigVMPin* Pin, const FProperty* HostProperty, uint8* HostMemory, const FProperty* InstanceProperty, uint8* InstanceMemory) {

		// copy the live memory from the host into the node instance
		URigVMMemoryStorage::CopyProperty(InstanceProperty, InstanceMemory, HostProperty, HostMemory);
	}, InSliceIndex);
	
	return NodeInstance;
}

bool URigVMUnitNode::UpdateHostFromStructInstance(URigVMHost* InHost, TSharedPtr<FStructOnScope> InInstance, int32 InSliceIndex) const
{
	check(InHost);
	check(InInstance.IsValid() && InInstance->IsValid());

	bool bChangedSomething = false;
	
	EnumeratePropertiesOnHostAndStructInstance(InHost, InInstance, true,
	[&bChangedSomething]
	(const URigVMPin* Pin, const FProperty* HostProperty, uint8* HostMemory, const FProperty* InstanceProperty, uint8* InstanceMemory) {

		// only update registers for inputs
		if(Pin->GetDirection() != ERigVMPinDirection::Input &&
			Pin->GetDirection() != ERigVMPinDirection::IO &&
			Pin->GetDirection() != ERigVMPinDirection::Visible)
		{
			return;
		}

		// only update registers for pins which don't have a top level link
		if(!Pin->GetRootPin()->GetLinkedSourcePins().IsEmpty())
		{
			return;
		}

		// ignore properties with matching data
		if(HostProperty->SameType(InstanceProperty))
		{
			if(HostProperty->Identical(HostMemory, InstanceMemory))
			{
				return;
			}
		}

		// copy the memory back to the host
		FRigVMMemoryStorageStruct::CopyProperty(HostProperty, HostMemory, InstanceProperty, InstanceMemory);
		bChangedSomething = true;

	}, InSliceIndex);

	return bChangedSomething;
}

void URigVMUnitNode::ComputePinValueDifferences(TSharedPtr<FStructOnScope> InCurrentInstance, TSharedPtr<FStructOnScope> InDesiredInstance, TMap<FString, FString>& OutNewPinDefaultValues) const
{
	check(InCurrentInstance.IsValid() && InCurrentInstance->IsValid());
	check(InDesiredInstance.IsValid() && InDesiredInstance->IsValid());
	
	const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InCurrentInstance->GetStruct());
	check(ScriptStruct == Cast<UScriptStruct>(InDesiredInstance->GetStruct()));
	
	for(URigVMPin* Pin : GetPins())
	{
		if(Pin->GetDirection() != ERigVMPinDirection::Input &&
			Pin->GetDirection() != ERigVMPinDirection::IO &&
			Pin->GetDirection() != ERigVMPinDirection::Visible)
		{
			continue;
		}

		const FProperty* Property = ScriptStruct->FindPropertyByName(Pin->GetFName());
		if(Property == nullptr)
		{
			continue;
		}

		const uint8* CurrentMemory = Property->ContainerPtrToValuePtr<uint8>(InCurrentInstance->GetStructMemory());
		const uint8* DesiredMemory = Property->ContainerPtrToValuePtr<uint8>(InDesiredInstance->GetStructMemory());
		if(Property->Identical(CurrentMemory, DesiredMemory))
		{
			continue;
		}

		FString DefaultValue;
		Property->ExportText_Direct(
			DefaultValue,
			DesiredMemory,
			DesiredMemory,
			nullptr,
			PPF_None,
			nullptr
		);

		if(!DefaultValue.IsEmpty())
		{
			OutNewPinDefaultValues.Add(Property->GetName(), DefaultValue);
		}
	}
}

void URigVMUnitNode::ComputePinValueDifferences(TSharedPtr<FStructOnScope> InDesiredInstance, TMap<FString, FString>& OutNewPinDefaultValues) const
{
	const TSharedPtr<FStructOnScope> DefaultInstance = ConstructStructInstance(false);
	ComputePinValueDifferences(DefaultInstance, InDesiredInstance, OutNewPinDefaultValues);
}

bool URigVMUnitNode::IsPartOfRuntime() const
{
	if(const URigVMBlueprint* Blueprint = GetTypedOuter<URigVMBlueprint>())
	{
		if(URigVMHost* DebuggedHost = Cast<URigVMHost>(Blueprint->GetObjectBeingDebugged()))
		{
			return IsPartOfRuntime(DebuggedHost);
		}
	}
	return false;
}

bool URigVMUnitNode::IsPartOfRuntime(URigVMHost* InHost) const
{
	check(InHost);
	if(URigVM* VM = InHost->GetVM())
	{
		const FRigVMByteCode& ByteCode = VM->GetByteCode();
		return ByteCode.GetFirstInstructionIndexForSubject(const_cast<URigVMUnitNode*>(this)) != INDEX_NONE;
	}
	return false;
}

FRigVMStructUpgradeInfo URigVMUnitNode::GetUpgradeInfo() const
{
	if(UScriptStruct* Struct = GetScriptStruct())
	{
		check(Struct->IsChildOf(FRigVMStruct::StaticStruct()));

		const TSharedPtr<FStructOnScope> StructOnScope = ConstructStructInstance();
		const FRigVMStruct* StructMemory = (const FRigVMStruct*)StructOnScope->GetStructMemory();
		return StructMemory->GetUpgradeInfo();
	}
	return FRigVMStructUpgradeInfo();
}

uint32 URigVMUnitNode::GetStructureHash() const
{
	uint32 Hash = Super::GetStructureHash();
	if(const UScriptStruct* ScriptStruct = GetScriptStruct())
	{
		Hash = HashCombine(Hash, FRigVMRegistry::Get().GetHashForScriptStruct(ScriptStruct)); 
	}
	return Hash;
}

