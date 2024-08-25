// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackNodeEvent.h"
#include "AvaPlaybackNodeFlow.generated.h"

class FText;

/*
 * Node part of the Event Nodes that Controls the Flow of Events
 */
UCLASS(Abstract)
class AVALANCHEMEDIA_API UAvaPlaybackNodeFlow : public UAvaPlaybackNodeEvent
{
	GENERATED_BODY()

public:
	virtual FText GetNodeCategoryText() const override final;
	
	virtual int32 GetMinChildNodes() const override final { return 1; }
	virtual int32 GetMaxChildNodes() const override { return 1; }
};
