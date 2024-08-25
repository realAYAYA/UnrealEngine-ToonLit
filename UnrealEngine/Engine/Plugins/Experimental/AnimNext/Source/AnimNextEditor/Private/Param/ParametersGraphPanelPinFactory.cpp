// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParametersGraphPanelPinFactory.h"
#include "SGraphPinParamName.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"

namespace UE::AnimNext::Editor
{

FName FParametersGraphPanelPinFactory::GetFactoryName() const
{
	return TEXT("ParametersGraphPanelPinFactory");
}

TSharedPtr<SGraphPin> FParametersGraphPanelPinFactory::CreatePin_Internal(UEdGraphPin* InPin) const
{
	if (URigVMEdGraphNode* RigNode = Cast<URigVMEdGraphNode>(InPin->GetOwningNode()))
	{
		URigVMEdGraph* RigGraph = Cast<URigVMEdGraph>(RigNode->GetGraph());

		URigVMPin* ModelPin = RigNode->GetModelPinFromPinPath(InPin->GetName());
		if (ModelPin)
		{
			if(ModelPin->GetCustomWidgetName() == "ParamName")
			{
				const FString ParamTypeString = ModelPin->GetMetaData("AllowedParamType");
				FAnimNextParamType FilterType = FAnimNextParamType::FromString(ParamTypeString);
				return SNew(SGraphPinParamName, InPin)
					.ModelPin(ModelPin)
					.GraphNode(RigNode)
					.FilterType(FilterType);
			}
		}
	}
	else if(UEdGraphNode* EdGraphNode = InPin->GetOwningNode())
	{
		if(EdGraphNode->GetPinMetaData(InPin->GetFName(), "CustomWidget") == "ParamName")
		{
			const FString ParamTypeString = EdGraphNode->GetPinMetaData(InPin->GetFName(), "AllowedParamType");
			FAnimNextParamType FilterType = FAnimNextParamType::FromString(ParamTypeString);
			return SNew(SGraphPinParamName, InPin)
				.FilterType(FilterType);
		}
	}

	return FRigVMEdGraphPanelPinFactory::CreatePin_Internal(InPin);
}

}
