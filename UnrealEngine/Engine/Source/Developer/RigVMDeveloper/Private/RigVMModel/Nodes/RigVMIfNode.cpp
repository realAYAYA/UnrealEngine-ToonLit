// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMIfNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMIfNode)

const FString URigVMIfNode::IfName = TEXT("If");
const FString URigVMIfNode::ConditionName = TEXT("Condition");
const FString URigVMIfNode::TrueName = TEXT("True");
const FString URigVMIfNode::FalseName = TEXT("False");
const FString URigVMIfNode::ResultName = TEXT("Result");


FName URigVMIfNode::GetNotation() const
{
	static constexpr TCHAR Format[] = TEXT("%s(in %s, in %s, in %s, out %s)");
	static const FName Notation = *FString::Printf(Format, *IfName, *ConditionName, *TrueName, *FalseName, *ResultName);
	return Notation;
}

const FRigVMTemplate* URigVMIfNode::GetTemplate() const
{
	if(const FRigVMTemplate* SuperTemplate = Super::GetTemplate())
	{
		return SuperTemplate;
	}
	
	if(CachedTemplate == nullptr)
	{
		static const FRigVMTemplate* IfNodeTemplate = nullptr;
		if(IfNodeTemplate)
		{
			return IfNodeTemplate;
		}

		static const FName ConditionFName = *ConditionName;
		static const FName TrueFName = *TrueName;
		static const FName FalseFName = *FalseName;
		static const FName ResultFName = *ResultName;

		static TArray<FRigVMTemplateArgument> Arguments;
		if(Arguments.IsEmpty())
		{
			static const TArray<FRigVMTemplateArgument::ETypeCategory> Categories = {
				FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
				FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
			};
			Arguments.Reserve(4);
			Arguments.Emplace(ConditionFName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Bool);
			Arguments.Emplace(TrueFName, ERigVMPinDirection::Input, Categories);
			Arguments.Emplace(FalseFName, ERigVMPinDirection::Input, Categories);
			Arguments.Emplace(ResultFName, ERigVMPinDirection::Output, Categories);
		}

		FRigVMTemplateDelegates Delegates;
		Delegates.NewArgumentTypeDelegate = 
			FRigVMTemplate_NewArgumentTypeDelegate::CreateLambda([](const FRigVMTemplate*, const FName& InArgumentName, int32 InTypeIndex)
			{
				FRigVMTemplateTypeMap Types;

				if(InArgumentName == TrueFName || InArgumentName == FalseFName || InArgumentName == ResultFName)
				{
					Types.Add(ConditionFName, RigVMTypeUtils::TypeIndex::Bool);
					Types.Add(TrueFName, InTypeIndex);
					Types.Add(FalseFName, InTypeIndex);
					Types.Add(ResultFName, InTypeIndex);
				}

				return Types;
			});

		IfNodeTemplate = CachedTemplate = FRigVMRegistry::Get().GetOrAddTemplateFromArguments(*IfName, Arguments, Delegates);
	}
	return CachedTemplate;
}


