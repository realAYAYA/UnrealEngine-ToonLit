// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackEditorGraphNode.h"
#include "AvaPlaybackEditorGraphNode_EventTrigger.generated.h"

UCLASS()
class UAvaPlaybackEditorGraphNode_EventTrigger : public UAvaPlaybackEditorGraphNode
{
	GENERATED_BODY()

public:
	
	virtual TSubclassOf<UAvaPlaybackNode> GetPlaybackNodeClass() const override;
	virtual FName GetOutputPinCategory() const override;

	//UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	//~UEdGraphNode interface
};
