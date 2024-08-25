// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackEditorGraphNode.h"
#include "AvaPlaybackEditorGraphNode_EventFlow.generated.h"

UCLASS()
class UAvaPlaybackEditorGraphNode_EventFlow : public UAvaPlaybackEditorGraphNode
{
	GENERATED_BODY()

public:
	
	virtual TSubclassOf<UAvaPlaybackNode> GetPlaybackNodeClass() const override;

	virtual FName GetInputPinCategory(int32 InputPinIndex) const override;
	virtual FName GetOutputPinCategory() const override;
};
