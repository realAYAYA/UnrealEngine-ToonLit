// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/RigVMDispatch_GetParameter.h"

#include "Param/ParametersExecuteContext.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVM.h"
#include "Graph/GraphExecuteContext.h"
#include "Context.h"
#include "Param/ParamStack.h"
#include "Param/ParamStack.h"

class UAnimNextParameter;

const FName FRigVMDispatch_GetParameter::ValueName = TEXT("Value");
const FName FRigVMDispatch_GetParameter::TypeHandleName = TEXT("Type");
const FName FRigVMDispatch_GetParameter::ParameterName = TEXT("Parameter");
const FName FRigVMDispatch_GetParameter::ParameterIdName = TEXT("ParameterId");

FRigVMDispatch_GetParameter::FRigVMDispatch_GetParameter()
{
	FactoryScriptStruct = StaticStruct();
}

FName FRigVMDispatch_GetParameter::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
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
FString FRigVMDispatch_GetParameter::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if (InArgumentName == ParameterIdName)
	{
		return FString::Printf(TEXT("%u"), 0xffffffff);
	}

	return Super::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}
#endif

const TArray<FRigVMTemplateArgument>& FRigVMDispatch_GetParameter::GetArguments() const
{
	static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories =
	{
		FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
		FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
	};

	static const TArray<FRigVMTemplateArgument> Arguments = {
		FRigVMTemplateArgument(ParameterName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName),
		FRigVMTemplateArgument(ValueName, ERigVMPinDirection::Output, ValueCategories),
		FRigVMTemplateArgument(ParameterIdName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32),
		FRigVMTemplateArgument(TypeHandleName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::UInt32),
	};

	return Arguments;
}

FRigVMTemplateTypeMap FRigVMDispatch_GetParameter::OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(ValueName, InTypeIndex);
	return Types;
}

void FRigVMDispatch_GetParameter::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	using namespace UE::AnimNext;

	const FName Parameter = *(FName*)Handles[0].GetData();
	const FProperty* ValueProperty = Handles[1].GetResolvedProperty();
	check(ValueProperty);
	uint8* TargetData = Handles[1].GetData();

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

	FAnimNextGraphExecuteContext& Context = InContext.GetPublicData<FAnimNextGraphExecuteContext>();
	TConstArrayView<uint8> SourceData;
	if (Context.GetContext().GetParamStack().GetParamData(FParamId(ParameterId), FParamTypeHandle::FromRaw(TypeHandle), SourceData) == FParamStack::EGetParamResult::Succeeded)
	{
		ValueProperty->CopyCompleteValue(TargetData, SourceData.GetData());
	}
}

