// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/RigVMDispatch_Switch.h"
#include "RigVMStringUtils.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMCore/RigVMStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDispatch_Switch)

const FName FRigVMDispatch_SwitchInt32::IndexName = TEXT("Index");
const FName FRigVMDispatch_SwitchInt32::CasesName = TEXT("Cases");

FName FRigVMDispatch_SwitchInt32::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] = {
		IndexName,
		FRigVMStruct::ControlFlowBlockToRunName
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_SwitchInt32::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Arguments;
	if(Arguments.IsEmpty())
	{
		Arguments.Emplace(IndexName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Int32);
		Arguments.Emplace(FRigVMStruct::ControlFlowBlockToRunName, ERigVMPinDirection::Hidden, RigVMTypeUtils::TypeIndex::FName);
	}
	return Arguments;
}

const TArray<FRigVMExecuteArgument>& FRigVMDispatch_SwitchInt32::GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const
{
	static const TArray<FRigVMExecuteArgument> Arguments =
	{
		{FRigVMStruct::ExecuteContextName, ERigVMPinDirection::Input},
		{CasesName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::ExecuteArray},
		{FRigVMStruct::ControlFlowCompletedName, ERigVMPinDirection::Output}
	};
	return Arguments;
}

#if WITH_EDITOR

FString FRigVMDispatch_SwitchInt32::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if(InArgumentName == CasesName)
	{
		if(InMetaDataKey == FRigVMStruct::FixedSizeArrayMetaName)
		{
			return TrueString;
		}
	}
	return FRigVMDispatch_CoreBase::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}

FString FRigVMDispatch_SwitchInt32::GetArgumentDefaultValue(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	if(InArgumentName == CasesName)
	{
		static const FString TwoCases = TEXT("((),())");
		return TwoCases;
	}
	return FRigVMDispatch_CoreBase::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}

FName FRigVMDispatch_SwitchInt32::GetDisplayNameForArgument(const FName& InArgumentName) const
{
	static const FString CasesPrefix = CasesName.ToString() + TEXT(".");
	const FString ArgumentNameString = InArgumentName.ToString();
	if(ArgumentNameString.StartsWith(CasesPrefix))
	{
		FString Left, Right;
		verify(RigVMStringUtils::SplitPinPathAtEnd(ArgumentNameString, Left, Right));
		if(Right.IsNumeric())
		{
			const int32 CaseIndex = FCString::Atoi(*Right);
			return GetCaseDisplayName(CaseIndex);
		}
	}
	return FRigVMDispatch_CoreBase::GetDisplayNameForArgument(InArgumentName);
}

#endif

const TArray<FName>& FRigVMDispatch_SwitchInt32::GetControlFlowBlocks_Impl(const FRigVMDispatchContext& InContext) const
{
	static const TArray<FName> DefaultBlocks = {FRigVMStruct::ControlFlowCompletedName};
	
	if(!InContext.StringRepresentation.IsEmpty())
	{
		static const FString CasesPrefix = CasesName.ToString() + TEXT("=");
		TArray<FString> NameValuePairs = RigVMStringUtils::SplitDefaultValue(InContext.StringRepresentation);
		for(const FString& NameValuePair : NameValuePairs)
		{
			if(NameValuePair.StartsWith(CasesPrefix))
			{
				const FString Values = NameValuePair.Mid(CasesPrefix.Len());

				static TMap<FString, TArray<FName>> BlockCache;
				if(const TArray<FName>* ExistingBlocks = BlockCache.Find(Values))
				{
					return *ExistingBlocks;
				}
				
				TArray<FName>& Blocks = BlockCache.Add(Values);
				
				const TArray<FString> CaseNames = RigVMStringUtils::SplitDefaultValue(Values);
				for(int32 CaseIndex = 0; CaseIndex < CaseNames.Num(); CaseIndex++)
				{
					Blocks.Add(GetCaseName(CaseIndex));
				}
				
				Blocks.Append(DefaultBlocks);
				return Blocks;
			}
		}
	}

	return DefaultBlocks;
}

FRigVMFunctionPtr FRigVMDispatch_SwitchInt32::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	return &FRigVMDispatch_SwitchInt32::Execute;
}

void FRigVMDispatch_SwitchInt32::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
{
#if WITH_EDITOR
	check(Handles[0].IsInt32());
	check(Handles[1].IsName());
#endif

	const int32 Index = *(const int32*)Handles[0].GetData();
	FName& BlockToRun = *(FName*)Handles[1].GetData();

	if(BlockToRun.IsNone())
	{
		BlockToRun = GetCaseName(Index);
	}
	else
	{
		BlockToRun = FRigVMStruct::ControlFlowCompletedName;
	}
}

FName FRigVMDispatch_SwitchInt32::GetCaseName(int32 InIndex)
{
	return FRigVMBranchInfo::GetFixedArrayLabel(CasesName, *FString::FromInt(InIndex));
}

FName FRigVMDispatch_SwitchInt32::GetCaseDisplayName(int32 InIndex)
{
	static constexpr TCHAR Format[] = TEXT("Case %d");
	return *FString::Printf(Format, InIndex);
}

