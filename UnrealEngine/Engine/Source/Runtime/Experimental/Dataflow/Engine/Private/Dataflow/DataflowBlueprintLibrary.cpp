// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowBlueprintLibrary.h"

#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowBlueprintLibrary)

void UDataflowBlueprintLibrary::EvaluateTerminalNodeByName(UDataflow* Dataflow, FName TerminalNodeName, UObject* ResultAsset)
{
	if (Dataflow && Dataflow->Dataflow)
	{
		TSharedPtr<FDataflowNode> Node = Dataflow->Dataflow->FindTerminalNode(TerminalNodeName);
		if (Node)
		{
			if (const FDataflowTerminalNode* TerminalNode = Node->AsType<const FDataflowTerminalNode>())
			{
				Dataflow::FEngineContext Context(ResultAsset, Dataflow, FPlatformTime::Cycles64());
				TerminalNode->Evaluate(Context);
				if (ResultAsset)
				{
					TerminalNode->SetAssetValue(ResultAsset, Context);
				}
			}
		}
		else
		{
			UE_LOG(LogChaos, Warning, TEXT("EvaluateTerminalNodeByName : Could not find terminal node : [%s], skipping evaluation"), *TerminalNodeName.ToString());
		}
	}
}













