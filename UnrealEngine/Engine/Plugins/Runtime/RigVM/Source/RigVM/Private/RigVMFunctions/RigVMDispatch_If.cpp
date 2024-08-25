// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/RigVMDispatch_If.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDispatch_If)

const FName FRigVMDispatch_If::ConditionName = TEXT("Condition");
const FName FRigVMDispatch_If::TrueName = TEXT("True");
const FName FRigVMDispatch_If::FalseName = TEXT("False");
const FName FRigVMDispatch_If::ResultName = TEXT("Result");

FName FRigVMDispatch_If::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] = {
		ConditionName,
		TrueName,
		FalseName,
		ResultName
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_If::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Infos;
	if(Infos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories = {
			FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
		};
		
		Infos.Emplace(ConditionName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Bool);
		Infos.Emplace(TrueName, ERigVMPinDirection::Input, ValueCategories);
		Infos.Emplace(FalseName, ERigVMPinDirection::Input, ValueCategories);
		Infos.Emplace(ResultName, ERigVMPinDirection::Output, ValueCategories);
	}
	return Infos;
}

FRigVMTemplateTypeMap FRigVMDispatch_If::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(ConditionName, RigVMTypeUtils::TypeIndex::Bool);
	Types.Add(TrueName, InTypeIndex);
	Types.Add(FalseName, InTypeIndex);
	Types.Add(ResultName, InTypeIndex);
	return Types;
}

#if WITH_EDITOR

FString FRigVMDispatch_If::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if(InArgumentName == TrueName || InArgumentName == FalseName)
	{
		if(InMetaDataKey == FRigVMStruct::ComputeLazilyMetaName)
		{
			return TrueString;
		}
	}
	return FRigVMDispatch_CoreBase::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}

#endif

FRigVMFunctionPtr FRigVMDispatch_If::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	return &FRigVMDispatch_If::Execute;
}

void FRigVMDispatch_If::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
{
	const FProperty* Property = Handles[1].GetResolvedProperty(); 

#if WITH_EDITOR
	check(Handles[0].IsBool());
#endif

	const bool& Condition = *(const bool*)Handles[0].GetData();

	FRigVMMemoryHandle& InputHandle = Condition ? Handles[1] : Handles[2];
	if(InputHandle.IsLazy())
	{
		InputHandle.ComputeLazyValueIfNecessary(InContext, InContext.GetSliceHash());
	}

	const uint8* Input = InputHandle.GetData();
	uint8* Result = Handles[3].GetData();

	(void)CopyProperty(
		Handles[3].GetProperty(), Result,
		InputHandle.GetProperty(), Input);
}

