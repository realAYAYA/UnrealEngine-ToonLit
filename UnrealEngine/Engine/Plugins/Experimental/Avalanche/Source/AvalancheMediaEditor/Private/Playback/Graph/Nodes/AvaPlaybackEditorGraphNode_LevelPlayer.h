// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackEditorGraphNode.h"
#include "AvaPlaybackEditorGraphNode_LevelPlayer.generated.h"

UCLASS()
class UAvaPlaybackEditorGraphNode_LevelPlayer : public UAvaPlaybackEditorGraphNode
{
	GENERATED_BODY()
	
	virtual TSubclassOf<UAvaPlaybackNode> GetPlaybackNodeClass() const override;
	virtual FName GetInputPinCategory(int32 InputPinIndex) const override;
	
	//UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	//~UEdGraphNode interface
};
