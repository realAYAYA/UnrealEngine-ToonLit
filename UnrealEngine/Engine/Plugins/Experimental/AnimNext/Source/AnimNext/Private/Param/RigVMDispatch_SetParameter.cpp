// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/RigVMDispatch_SetParameter.h"

#include "Param/ParametersExecuteContext.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVM.h"
#include "Graph/GraphExecuteContext.h"
#include "Context.h"
#include "Param/ParamStack.h"
#include "Param/ParamStack.h"

class UAnimNextParameter;

const FName FRigVMDispatch_SetParameter::ExecuteContextName = TEXT("ExecuteContext");
const FName FRigVMDispatch_SetParameter::ValueName = TEXT("Value");
const FName FRigVMDispatch_SetParameter::TypeHandleName = TEXT("Type");
const FName FRigVMDispatch_SetParameter::ParameterName = TEXT("Parameter");
const FName FRigVMDispatch_SetParameter::ParameterIdName = TEXT("ParameterId");

FRigVMDispatch_SetParameter::FRigVMDispatch_SetParameter()
{
	FactoryScriptStruct = StaticStruct();
}

FName FRigVMDispatch_SetParameter::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] = 
	{
		ParameterName,
		ValueName,
		ParameterIdName,
		TypeHandleName
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

#if WITH_EDITOR
FString FRigVMDispatch_SetParameter::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if (InArgumentName == ParameterIdName)
	{
		return FString::Printf(TEXT("%u"), 0xffffffff);
	}

	return Super::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}
#endif

const TArray<FRigVMTemplateArgument>& FRigVMDispatch_SetParameter::GetArguments() const
{
	static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories =
	{
		FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
		FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
	};
	
	static const TArray<FRigVMTemplateArgument> Arguments = { 
		FRigVMTemplateArgument(ParameterName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName),
		FRigVMTemplateArgument(ValueName, ERigVMPinDirection::Input, ValueCategories),
		FRigVMTemplateArgument(ParameterIdName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32),
		FRigVMTemplateArgument(TypeHandleName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32),
	};

	return Arguments;
}

TArray<FRigVMExecuteArgument>& FRigVMDispatch_SetParameter::GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const
{
	static TArray<FRigVMExecuteArgument> Arguments = { { ExecuteContextName, ERigVMPinDirection::Input } };
	return Arguments;
}

FRigVMTemplateTypeMap FRigVMDispatch_SetParameter::OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(ValueName, InTypeIndex);
	return Types;
}

void FRigVMDispatch_SetParameter::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	using namespace UE::AnimNext;

	const FName Parameter = *(FName*)Handles[0].GetData();
	const FProperty* ValueProperty = Handles[1].GetResolvedProperty();
	check(ValueProperty);
	const uint8* SourceData =  Handles[1].GetData();

	uint32& ParameterId = *(uint32*)Handles[2].GetData();
	if (ParameterId != FParamId::InvalidIndex)
	{
		ParameterId = FParamId(Parameter).ToInt();
	}

	uint32& TypeHandle = *(uint32*)Handles[3].GetData();
	if (TypeHandle == 0)
	{
		TypeHandle = FParamTypeHandle::FromProperty(ValueProperty).ToRaw();
	}

	const FAnimNextGraphExecuteContext& Context = InContext.GetPublicData<FAnimNextGraphExecuteContext>();
	TArrayView<uint8> TargetData;
	if (Context.GetContext().GetMutableParamStack().GetMutableParamData(FParamId(ParameterId), FParamTypeHandle::FromRaw(TypeHandle), TargetData) == FParamStack::EGetParamResult::Succeeded)
	{
		ValueProperty->CopyCompleteValue(TargetData.GetData(), SourceData);
	}	
}

