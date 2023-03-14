// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMSelectNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMSelectNode)

const FString URigVMSelectNode::SelectName = TEXT("Select");
const FString URigVMSelectNode::IndexName = TEXT("Index");
const FString URigVMSelectNode::ValueName = TEXT("Values");
const FString URigVMSelectNode::ResultName = TEXT("Result");

bool URigVMSelectNode::AllowsLinksOn(const URigVMPin* InPin) const
{
	if(InPin->GetRootPin() == InPin)
	{
		if(InPin->GetName() == ValueName)
		{
			return false;
		}
	}

	return true;
}

FName URigVMSelectNode::GetNotation() const
{
	static constexpr TCHAR Format[] = TEXT("%s(in %s, in %s, out %s)");
	static const FName Notation = *FString::Printf(Format, *SelectName, *IndexName, *ValueName, *ResultName);
	return Notation;
}

const FRigVMTemplate* URigVMSelectNode::GetTemplate() const
{
	if(const FRigVMTemplate* SuperTemplate = Super::GetTemplate())
	{
		return SuperTemplate;
	}
	
	if(CachedTemplate == nullptr)
	{
		static const FRigVMTemplate* SelectNodeTemplate = nullptr;
		if(SelectNodeTemplate)
		{
			return SelectNodeTemplate;
		}

		static const FName IndexFName = *IndexName;
		static const FName ValueFName = *ValueName;
		static const FName ResultFName = *ResultName;

		static TArray<FRigVMTemplateArgument> Arguments;
		if(Arguments.IsEmpty())
		{
			static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueTypeCategories = {
				FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue,
				FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue
			};
			static const TArray<FRigVMTemplateArgument::ETypeCategory> ResultTypeCategories = {
				FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
				FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
			};
			Arguments.Reserve(3);
			Arguments.Emplace(IndexFName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Int32);
			Arguments.Emplace(ValueFName, ERigVMPinDirection::Input, ValueTypeCategories);
			Arguments.Emplace(ResultFName, ERigVMPinDirection::Output, ResultTypeCategories);
		}
		
		FRigVMTemplateDelegates Delegates;
		Delegates.NewArgumentTypeDelegate = 
			FRigVMTemplate_NewArgumentTypeDelegate::CreateLambda([](const FRigVMTemplate*, const FName& InArgumentName, int32 InTypeIndex)
			{
				FRigVMTemplateTypeMap Types;

				int32 ValueTypeIndex = INDEX_NONE;
				int32 ResultTypeIndex = INDEX_NONE;

				if(InArgumentName == ValueFName)
				{
					ValueTypeIndex = InTypeIndex;
					ResultTypeIndex = FRigVMRegistry::Get().GetBaseTypeFromArrayTypeIndex(InTypeIndex);
				}
				else if(InArgumentName == ResultFName)
				{
					ValueTypeIndex = FRigVMRegistry::Get().GetArrayTypeFromBaseTypeIndex(InTypeIndex);;
					ResultTypeIndex = InTypeIndex;
				}
				
				if(ValueTypeIndex != INDEX_NONE && ResultTypeIndex != INDEX_NONE)
				{
					Types.Add(IndexFName, RigVMTypeUtils::TypeIndex::Int32);
					Types.Add(ValueFName, ValueTypeIndex);
					Types.Add(ResultFName, ResultTypeIndex);
				}

				return Types;
			});

		SelectNodeTemplate = CachedTemplate = FRigVMRegistry::Get().GetOrAddTemplateFromArguments(*SelectName, Arguments, Delegates);
	}
	return CachedTemplate;
}
