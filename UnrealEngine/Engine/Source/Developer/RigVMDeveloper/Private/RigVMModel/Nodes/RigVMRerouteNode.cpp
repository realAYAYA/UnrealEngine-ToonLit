// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMRerouteNode.h"
#include "RigVMModel/RigVMGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMRerouteNode)

const FString URigVMRerouteNode::RerouteName = TEXT("Reroute");
const FString URigVMRerouteNode::ValueName = TEXT("Value");

URigVMRerouteNode::URigVMRerouteNode()
: bShowAsFullNode(true)
{
}

FString URigVMRerouteNode::GetNodeTitle() const
{
	if(const URigVMPin* ValuePin = FindPin(ValueName))
	{
		FString TypeDisplayName;
		if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ValuePin->GetCPPTypeObject()))
		{
			TypeDisplayName = ScriptStruct->GetDisplayNameText().ToString();
		}
		else if(const UEnum* Enum = Cast<UEnum>(ValuePin->GetCPPTypeObject()))
		{
			TypeDisplayName = Enum->GetName();
		}
		else if(const UClass* Class = Cast<UClass>(ValuePin->GetCPPTypeObject()))
		{
			TypeDisplayName = Class->GetDisplayNameText().ToString();
		}
		else if(ValuePin->IsArray())
		{
			TypeDisplayName = ValuePin->GetArrayElementCppType();
		}
		else
		{
			TypeDisplayName = ValuePin->GetCPPType();
		}

		if(TypeDisplayName.IsEmpty())
		{
			return RerouteName;
		}

		TypeDisplayName = TypeDisplayName.Left(1).ToUpper() + TypeDisplayName.Mid(1);

		if(ValuePin->IsArray())
		{
			TypeDisplayName += TEXT(" Array");
		}

		return TypeDisplayName;
	}
	return RerouteName;
}

bool URigVMRerouteNode::GetShowsAsFullNode() const
{
	return bShowAsFullNode;
}

FLinearColor URigVMRerouteNode::GetNodeColor() const
{
	return FLinearColor::White;
}

FName URigVMRerouteNode::GetNotation() const
{
	static constexpr TCHAR Format[] = TEXT("%s(io %s)");
	static const FName RerouteNotation = *FString::Printf(Format, *RerouteName, *ValueName);
	return RerouteNotation;
}

const FRigVMTemplate* URigVMRerouteNode::GetTemplate() const
{
	if(const FRigVMTemplate* SuperTemplate = Super::GetTemplate())
	{
		return SuperTemplate;
	}
	
	if(CachedTemplate == nullptr)
	{
		static const FRigVMTemplate* RerouteNodeTemplate = nullptr;
		if(RerouteNodeTemplate)
		{
			return RerouteNodeTemplate;
		}

		static TArray<FRigVMTemplateArgument> Arguments;
		if(Arguments.IsEmpty())
		{
			static const TArray<FRigVMTemplateArgument::ETypeCategory> Categories = {
				FRigVMTemplateArgument::ETypeCategory_Execute,
				FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
				FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
			};
			Arguments.Emplace(TEXT("Value"), ERigVMPinDirection::IO, Categories);
		}

		FRigVMTemplateDelegates Delegates;
		Delegates.NewArgumentTypeDelegate = 
			FRigVMTemplate_NewArgumentTypeDelegate::CreateLambda([](const FRigVMTemplate*, const FName& InArgumentName, int32 InTypeIndex)
			{
				// since a reroute has only one argument this is simple
				FRigVMTemplateTypeMap Types;
				Types.Add(InArgumentName, InTypeIndex);
				return Types;
			});

		RerouteNodeTemplate = CachedTemplate = FRigVMRegistry::Get().GetOrAddTemplateFromArguments(*RerouteName, Arguments, Delegates);
	}
	return CachedTemplate;
}

