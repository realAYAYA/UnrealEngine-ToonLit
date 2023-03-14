// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMDispatchNode.h"
#include "RigVMUserWorkflowRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDispatchNode)

FString URigVMDispatchNode::GetNodeTitle() const
{
	if (const FRigVMDispatchFactory* Factory = GetFactory())
	{
		return Factory->GetNodeTitle(GetFilteredTypes());
	}
	return Super::GetNodeTitle();
}

FText URigVMDispatchNode::GetToolTipText() const
{
	if (const FRigVMDispatchFactory* Factory = GetFactory())
	{
		return Factory->GetNodeTooltip(GetFilteredTypes());
	}
	return URigVMNode::GetToolTipText();
}

FLinearColor URigVMDispatchNode::GetNodeColor() const
{
	if (const FRigVMDispatchFactory* Factory = GetFactory())
	{
		return Factory->GetNodeColor();
	}
	return URigVMNode::GetNodeColor();
}

bool URigVMDispatchNode::IsDefinedAsConstant() const
{
	if (const UScriptStruct* Struct = GetFactoryStruct())
	{
		return Struct->HasMetaData(FRigVMStruct::ConstantMetaName);
	}
	return false;
}

bool URigVMDispatchNode::IsDefinedAsVarying() const
{
	if (const UScriptStruct* Struct = GetFactoryStruct())
	{
		return Struct->HasMetaData(FRigVMStruct::VaryingMetaName);
	}
	return false;
}

FText URigVMDispatchNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	if (const FRigVMDispatchFactory* Factory = GetFactory())
	{
		const URigVMPin* RootPin = InPin->GetRootPin();
		return Factory->GetArgumentTooltip(RootPin->GetFName(), RootPin->GetTypeIndex());
	}
	return URigVMNode::GetToolTipTextForPin(InPin);
}

bool URigVMDispatchNode::IsDeprecated() const
{
	return !GetDeprecatedMetadata().IsEmpty();
}

FString URigVMDispatchNode::GetDeprecatedMetadata() const
{
	if (const UScriptStruct* Struct = GetFactoryStruct())
	{
		FString DeprecatedMetadata;
		if(Struct->GetStringMetaDataHierarchical(FRigVMStruct::DeprecatedMetaName, &DeprecatedMetadata))
		{
			return DeprecatedMetadata;
		}
	}
	return FString();
}

TArray<URigVMPin*> URigVMDispatchNode::GetAggregateInputs() const
{
	TArray<URigVMPin*> AggregateInputs;
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	if(const FRigVMDispatchFactory* Factory = GetFactory())
	{
		const TArray<FName> ArgumentNames = Factory->GetAggregateInputArguments();
		for(const FName& ArgumentName : ArgumentNames)
		{
			AggregateInputs.Add(FindPin(ArgumentName.ToString()));
		}
	}
#endif
	return AggregateInputs;
}

TArray<URigVMPin*> URigVMDispatchNode::GetAggregateOutputs() const
{
	TArray<URigVMPin*> AggregateOutputs;
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	if(const FRigVMDispatchFactory* Factory = GetFactory())
	{
		const TArray<FName> ArgumentNames = Factory->GetAggregateOutputArguments();
		for(const FName& ArgumentName : ArgumentNames)
		{
			AggregateOutputs.Add(FindPin(ArgumentName.ToString()));
		}
	}
#endif
	return AggregateOutputs;
}

FName URigVMDispatchNode::GetNextAggregateName(const FName& InLastAggregatePinName) const
{
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	if(const FRigVMDispatchFactory* Factory = GetFactory())
	{
		return Factory->GetNextAggregateName(InLastAggregatePinName);
	}
#endif
	return FName();
}

const FRigVMDispatchFactory* URigVMDispatchNode::GetFactory() const
{
	if(CachedFactory)
	{
		return CachedFactory;
	}
	
	if(const FRigVMTemplate* Template = GetTemplate())
	{
		CachedFactory = Template->GetDispatchFactory(); 
		return CachedFactory;
	}

	// Try to find factories that have not been registered yet
	FRigVMRegistry::Get().RefreshEngineTypes();
	if(const FRigVMTemplate* Template = GetTemplate())
	{
		CachedFactory = Template->GetDispatchFactory(); 
		return CachedFactory;
	}
	
	return nullptr;
}

const UScriptStruct* URigVMDispatchNode::GetFactoryStruct() const
{
	if(const FRigVMDispatchFactory* Factory = GetFactory())
	{
		return Factory->GetScriptStruct();
	}
	return nullptr;
}

const FRigVMTemplateTypeMap& URigVMDispatchNode::GetFilteredTypes() const
{
	const TArray<int32> PermutationIndices = GetFilteredPermutationsIndices();
	if(PermutationIndices.Num() == 1)
	{
		TypesFromPins = GetTypesForPermutation(PermutationIndices[0]);
	}
	else
	{
		TypesFromPins.Reset();
	}
	return TypesFromPins;
}

void URigVMDispatchNode::InvalidateCache()
{
	Super::InvalidateCache();
	CachedFactory = nullptr;
}

FRigVMStructUpgradeInfo URigVMDispatchNode::GetUpgradeInfo() const
{
	if(const FRigVMDispatchFactory* Factory = GetFactory())
	{
		return Factory->GetUpgradeInfo(GetFilteredTypes());
	}
	return FRigVMStructUpgradeInfo();
}

