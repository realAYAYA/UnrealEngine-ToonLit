// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMIfNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMIfNode)

const FString UDEPRECATED_RigVMIfNode::IfName = TEXT("If");
const FString UDEPRECATED_RigVMIfNode::ConditionName = TEXT("Condition");
const FString UDEPRECATED_RigVMIfNode::TrueName = TEXT("True");
const FString UDEPRECATED_RigVMIfNode::FalseName = TEXT("False");
const FString UDEPRECATED_RigVMIfNode::ResultName = TEXT("Result");


FName UDEPRECATED_RigVMIfNode::GetNotation() const
{
	static constexpr TCHAR Format[] = TEXT("%s(in %s,in %s,in %s,out %s)");
	static const FName Notation = *FString::Printf(Format, *IfName, *ConditionName, *TrueName, *FalseName, *ResultName);
	return Notation;
}

const FRigVMTemplate* UDEPRECATED_RigVMIfNode::GetTemplate() const
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

		static TArray<FRigVMTemplateArgumentInfo> Infos;
		if(Infos.IsEmpty())
		{
			static const TArray<FRigVMTemplateArgument::ETypeCategory> Categories = {
				FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
				FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
			};
			Infos.Reserve(4);
			Infos.Emplace(ConditionFName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Bool);
			Infos.Emplace(TrueFName, ERigVMPinDirection::Input, Categories);
			Infos.Emplace(FalseFName, ERigVMPinDirection::Input, Categories);
			Infos.Emplace(ResultFName, ERigVMPinDirection::Output, Categories);
		}

		FRigVMTemplateDelegates Delegates;
		Delegates.NewArgumentTypeDelegate = 
			FRigVMTemplate_NewArgumentTypeDelegate::CreateLambda([](const FName& InArgumentName, int32 InTypeIndex)
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

		IfNodeTemplate = CachedTemplate = FRigVMRegistry::Get().GetOrAddTemplateFromArguments(*IfName, Infos, Delegates);
	}
	return CachedTemplate;
}


