// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "AvaPlaybackAction_PasteNode.generated.h"

USTRUCT()
struct FAvaPlaybackAction_PasteNode : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	FAvaPlaybackAction_PasteNode() = default;

	FAvaPlaybackAction_PasteNode(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
	{
	}

	//FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph
		, UEdGraphPin* FromPin
		, const FVector2D Location
		, bool bSelectNewNode = true) override;
	//~FEdGraphSchemaAction Interface
};
