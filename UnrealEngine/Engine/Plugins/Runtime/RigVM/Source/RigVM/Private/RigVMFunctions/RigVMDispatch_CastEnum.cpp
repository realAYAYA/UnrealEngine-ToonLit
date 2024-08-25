// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/RigVMDispatch_CastEnum.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMFunctions/RigVMFunctionDefines.h"

#define LOCTEXT_NAMESPACE "RigVMDispatch_CastEnum"

const FName FRigVMDispatch_CastEnumBase::ValueName = TEXT("Value");
const FName FRigVMDispatch_CastEnumBase::ResultName = TEXT("Result");

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_CastEnumToInt::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> OutInfos;
	if (OutInfos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ElementCategories =
		{
			FRigVMTemplateArgument::ETypeCategory_SingleEnumValue
		};
		
		static TArray<FRigVMTemplateArgumentInfo> Infos;
		Infos.Emplace(ValueName, ERigVMPinDirection::Input, ElementCategories);
		Infos.Emplace(ResultName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Int32);
		
		OutInfos = BuildArgumentListFromPrimaryArgument(Infos, ValueName);
	}

	return OutInfos;
}

bool FRigVMDispatch_CastEnumToInt::GetPermutationsFromArgumentType(const FName& InArgumentName, const TRigVMTypeIndex& InTypeIndex, TArray<FRigVMTemplateTypeMap, TInlineAllocator<1>>& OutPermutations) const
{
	if (InArgumentName == ValueName)
	{
		OutPermutations.Add(
	{
			{ ValueName, InTypeIndex },
			{ ResultName, RigVMTypeUtils::TypeIndex::Int32 }
		});
	}
	else if (InArgumentName == ResultName && InTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		const TArray<TRigVMTypeIndex>& EnumTypes = FRigVMRegistry::Get().GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue);
		for (const TRigVMTypeIndex& Type : EnumTypes)
		{
			OutPermutations.Add(
	{
				{ ValueName, Type },
				{ ResultName, InTypeIndex }
			});
		}
	}
	return !OutPermutations.IsEmpty();
}

#if WITH_EDITOR

FString FRigVMDispatch_CastEnumToInt::GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const
{
	return TEXT("Cast to int");
}

FText FRigVMDispatch_CastEnumToInt::GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const
{
	return LOCTEXT("CastEnumToolTip", "Casts from enum to int");
}

#endif

void FRigVMDispatch_CastEnumToInt::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	const FEnumProperty* ValueProperty = CastFieldChecked<FEnumProperty>(Handles[0].GetProperty());
	const FProperty* ResultProperty = CastFieldChecked<FProperty>(Handles[1].GetProperty());

	if (!ResultProperty || !ValueProperty)
	{
		return;
	}

	uint8* ValuePtr = Handles[0].GetData();
	int32* ResultPtr = (int32*)Handles[1].GetData();

	if (ValuePtr == nullptr || ResultPtr == nullptr)
	{
		return;
	}
	
	*ResultPtr = 0;
	ValueProperty->CopyCompleteValue(ResultPtr, ValuePtr);

#if WITH_EDITOR
	if (*ResultPtr == INDEX_NONE)
	{
		const FRigVMExecuteContext& ExecuteContext = InContext.GetPublicData<>();
		if(ExecuteContext.GetLog() != nullptr)
		{
			ExecuteContext.Report(EMessageSeverity::Error, InContext.GetPublicData<>().GetFunctionName(), InContext.GetPublicData<>().GetInstructionIndex(), TEXT("Enum value invalid"));
		}
	}
#endif
}



const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_CastIntToEnum::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> OutInfos;
	if (OutInfos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ElementCategories =
		{
			FRigVMTemplateArgument::ETypeCategory_SingleEnumValue
		};
		
		static TArray<FRigVMTemplateArgumentInfo> Infos;
		Infos.Emplace(ValueName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Int32);
		Infos.Emplace(ResultName, ERigVMPinDirection::Output, ElementCategories);
		
		OutInfos = BuildArgumentListFromPrimaryArgument(Infos, ResultName);
	}

	return OutInfos;
}

bool FRigVMDispatch_CastIntToEnum::GetPermutationsFromArgumentType(const FName& InArgumentName, const TRigVMTypeIndex& InTypeIndex, TArray<FRigVMTemplateTypeMap, TInlineAllocator<1>>& OutPermutations) const
{
	if (InArgumentName == ResultName)
	{
		OutPermutations.Add(
	{
			{ ValueName, RigVMTypeUtils::TypeIndex::Int32 },
			{ ResultName, InTypeIndex }
		});
	}
	else if (InArgumentName == ValueName && InTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		const TArray<TRigVMTypeIndex>& EnumTypes = FRigVMRegistry::Get().GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue);
		for (const TRigVMTypeIndex& Type : EnumTypes)
		{
			OutPermutations.Add(
	{
				{ ValueName, InTypeIndex },
				{ ResultName, Type }
			});
		}
	}
	return !OutPermutations.IsEmpty();
}

#if WITH_EDITOR

FString FRigVMDispatch_CastIntToEnum::GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const
{
	return TEXT("Cast to enum");
}

FText FRigVMDispatch_CastIntToEnum::GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const
{
	return LOCTEXT("CastIntToEnumToolTip", "Casts from int to enum");
}

#endif

void FRigVMDispatch_CastIntToEnum::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	const FProperty* ValueProperty = CastFieldChecked<FProperty>(Handles[0].GetProperty());
	const FEnumProperty* ResultProperty = CastFieldChecked<FEnumProperty>(Handles[1].GetProperty());

	if (!ResultProperty || !ValueProperty)
	{
		return;
	}

	int32* ValuePtr = (int32*)Handles[0].GetData();
	uint8* ResultPtr = Handles[1].GetData();

	if (ValuePtr == nullptr || ResultPtr == nullptr)
	{
		return;
	}
	
	*ResultPtr = 0;
	ResultProperty->CopyCompleteValue(ResultPtr, ValuePtr);
	
#if WITH_EDITOR
	if (*ResultPtr == INDEX_NONE)
	{
		const FRigVMExecuteContext& ExecuteContext = InContext.GetPublicData<>();
		if(ExecuteContext.GetLog() != nullptr)
		{
			ExecuteContext.Report(EMessageSeverity::Error, InContext.GetPublicData<>().GetFunctionName(), InContext.GetPublicData<>().GetInstructionIndex(), TEXT("Enum value invalid"));
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE
