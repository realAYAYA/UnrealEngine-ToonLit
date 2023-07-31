// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "VisualLoggerRenderingActorBase.h"
#include "VLogRenderingActor.generated.h"

/**
*	Transient actor used to draw visual logger data on level
*/

UCLASS(config = Engine, NotBlueprintable, Transient, notplaceable, AdvancedClassDisplay)
class AVLogRenderingActor : public AVisualLoggerRenderingActorBase 
{
public:
	GENERATED_UCLASS_BODY()
	virtual ~AVLogRenderingActor();
	void Reset();
	void AddLogEntry(const FVisualLogEntry& LogEntry);

	virtual void IterateDebugShapes(TFunction<void(const FTimelineDebugShapes&) > Callback) override;

private:
	FTimelineDebugShapes DebugShapes;
};
