// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/RigVMDispatch_Select.h"
#include "RigVMStringUtils.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDispatch_Select)

const FName FRigVMDispatch_SelectInt32::IndexName = TEXT("Index");
const FName FRigVMDispatch_SelectInt32::ValuesName = TEXT("Values");
const FName FRigVMDispatch_SelectInt32::ResultName = TEXT("Result");


FName FRigVMDispatch_SelectInt32::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	if(InOperandIndex > 0 && InOperandIndex < InTotalOperands - 1)
	{
		return FRigVMBranchInfo::GetFixedArrayLabel(ValuesName, *FString::FromInt(InOperandIndex - 1));
	}
	
	return InOperandIndex == 0 ? IndexName : ResultName;
}


const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_SelectInt32::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Infos;
	if (Infos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ArrayCategories = {
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue
		};
		static const TArray<FRigVMTemplateArgument::ETypeCategory> SingleCategories = {
			FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
		};
		
		Infos.Emplace(IndexName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Int32);
		Infos.Emplace(ValuesName, ERigVMPinDirection::Input, ArrayCategories);
		Infos.Emplace(ResultName, ERigVMPinDirection::Output, SingleCategories);
	}

	return Infos;
}

FRigVMTemplateTypeMap FRigVMDispatch_SelectInt32::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(IndexName, RigVMTypeUtils::TypeIndex::Int32);
	if(InArgumentName == ValuesName)
	{
		Types.Add(ValuesName, InTypeIndex);
		Types.Add(ResultName, FRigVMRegistry::Get().GetBaseTypeFromArrayTypeIndex(InTypeIndex));
	}
	else
	{
		Types.Add(ValuesName, FRigVMRegistry::Get().GetArrayTypeFromBaseTypeIndex(InTypeIndex));
		Types.Add(ResultName, InTypeIndex);
	}
	return Types;
}

#if WITH_EDITOR

FString FRigVMDispatch_SelectInt32::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if(InArgumentName == ValuesName)
	{
		if(InMetaDataKey == FRigVMStruct::ComputeLazilyMetaName)
		{
			return TrueString;
		}
		if(InMetaDataKey == FRigVMStruct::FixedSizeArrayMetaName)
		{
			return TrueString;
		}
	}
	return FRigVMDispatch_CoreBase::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}

FString FRigVMDispatch_SelectInt32::GetArgumentDefaultValue(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	if(InArgumentName == IndexName)
	{
		static const FString ZeroValue = TEXT("0");
		return ZeroValue;
	}
	if(InArgumentName == ValuesName)
	{
		const TRigVMTypeIndex ElementTypeIndex = FRigVMRegistry::Get().GetBaseTypeFromArrayTypeIndex(InTypeIndex);
		FString ElementDefaultValue = FRigVMDispatch_CoreBase::GetArgumentDefaultValue(InArgumentName, ElementTypeIndex);
		if(ElementDefaultValue.IsEmpty())
		{
			static const FString EmptyBracesString = TEXT("()");
			ElementDefaultValue = EmptyBracesString;
		}

		// default to two entries
		return RigVMStringUtils::JoinDefaultValue({ElementDefaultValue, ElementDefaultValue});
	}
	return FRigVMDispatch_CoreBase::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}

#endif


FRigVMFunctionPtr FRigVMDispatch_SelectInt32::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	return &FRigVMDispatch_SelectInt32::Execute;
}

void FRigVMDispatch_SelectInt32::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
{
	const FProperty* Property = Handles.Last().GetResolvedProperty();
	const int32 NumCases = Handles.Num() - 2;

	if(NumCases < 1)
	{
		static constexpr TCHAR InvalidIndexMessage[] = TEXT("SelectInt32: No cases to select from.");
		InContext.GetPublicData<>().Log(EMessageSeverity::Error, InvalidIndexMessage);
		return;
	}

#if WITH_EDITOR
	check(Handles[0].IsInt32());
#endif

	int32 Index = *(const int32*)Handles[0].GetData();
	if(!FMath::IsWithin(Index, 0, NumCases))
	{
		static constexpr TCHAR InvalidIndexMessage[] = TEXT("SelectInt32: Invalid index %d - num cases is %d. Falling back to index 0.");
		InContext.GetPublicData<>().Logf(EMessageSeverity::Warning, InvalidIndexMessage, Index, NumCases);
		Index = 0;
	}

	FRigVMMemoryHandle& InputHandle = Handles[1 + Index];
	if(InputHandle.IsLazy())
	{
		InputHandle.ComputeLazyValueIfNecessary(InContext, InContext.GetSliceHash());
	}

	const uint8* Input = InputHandle.GetData();
	uint8* Result = Handles.Last().GetData();

	// copy property performs compatibility check and will report accordingly
	(void)CopyProperty(
		Handles.Last().GetProperty(), Result,
		InputHandle.GetProperty(), Input);
}

