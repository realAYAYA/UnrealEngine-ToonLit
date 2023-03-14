// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNodeFactory.h"

#include "Dataflow/DataflowNode.h"
#include "Misc/MessageDialog.h"

namespace Dataflow
{
	FNodeFactory* FNodeFactory::Instance = nullptr;

	TSharedPtr<FDataflowNode> FNodeFactory::NewNodeFromRegisteredType(FGraph& Graph, const FNewNodeParameters& Param)
	{ 
		if (ClassMap.Contains(Param.Type))
		{
			TUniquePtr<FDataflowNode> Node = ClassMap[Param.Type](Param);
			if(Node->IsValid())
			{
				return Graph.AddNode(MoveTemp(Node));
			}
			
			const FText ErrorTitle = FText::FromString("Node Factory");
			const FString ErrorMessageString = FString::Printf(TEXT("Cannot create Node %s. Node Type %s is not well defined."), *Node->GetName().ToString(), *Node->GetDisplayName().ToString());
			const FText ErrorMessage = FText::FromString(ErrorMessageString);
			FMessageDialog::Debugf(ErrorMessage, &ErrorTitle);
		}
		return TSharedPtr<FDataflowNode>(nullptr);
	}
}

