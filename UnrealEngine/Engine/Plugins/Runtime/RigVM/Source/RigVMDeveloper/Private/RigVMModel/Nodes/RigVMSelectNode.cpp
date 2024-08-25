// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMSelectNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMSelectNode)

const FString UDEPRECATED_RigVMSelectNode::SelectName = TEXT("Select");
const FString UDEPRECATED_RigVMSelectNode::IndexName = TEXT("Index");
const FString UDEPRECATED_RigVMSelectNode::ValueName = TEXT("Values");
const FString UDEPRECATED_RigVMSelectNode::ResultName = TEXT("Result");

bool UDEPRECATED_RigVMSelectNode::AllowsLinksOn(const URigVMPin* InPin) const
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

FName UDEPRECATED_RigVMSelectNode::GetNotation() const
{
	static constexpr TCHAR Format[] = TEXT("%s(in %s,in %s,out %s)");
	static const FName Notation = *FString::Printf(Format, *SelectName, *IndexName, *ValueName, *ResultName);
	return Notation;
}

const FRigVMTemplate* UDEPRECATED_RigVMSelectNode::GetTemplate() const
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

		static TArray<FRigVMTemplateArgumentInfo> Infos;
		if(Infos.IsEmpty())
		{
			static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueTypeCategories = {
				FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue,
				FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue
			};
			static const TArray<FRigVMTemplateArgument::ETypeCategory> ResultTypeCategories = {
				FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
				FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
			};
			Infos.Reserve(3);
			Infos.Emplace(IndexFName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::Int32);
			Infos.Emplace(ValueFName, ERigVMPinDirection::Input, ValueTypeCategories);
			Infos.Emplace(ResultFName, ERigVMPinDirection::Output, ResultTypeCategories);
		}
		
		FRigVMTemplateDelegates Delegates;
		Delegates.NewArgumentTypeDelegate = 
			FRigVMTemplate_NewArgumentTypeDelegate::CreateLambda([](const FName& InArgumentName, int32 InTypeIndex)
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

		SelectNodeTemplate = CachedTemplate = FRigVMRegistry::Get().GetOrAddTemplateFromArguments(*SelectName, Infos, Delegates);
	}
	return CachedTemplate;
}
