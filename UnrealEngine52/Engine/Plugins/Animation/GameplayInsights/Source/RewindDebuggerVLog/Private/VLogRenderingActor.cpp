// Copyright Epic Games, Inc. All Rights Reserved.

#include "VLogRenderingActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VLogRenderingActor)

AVLogRenderingActor::AVLogRenderingActor(const FObjectInitializer& ObjectInitializer)
{
}

AVLogRenderingActor::~AVLogRenderingActor()
{
}

void AVLogRenderingActor::Reset()
{
 	DebugShapes.Reset();
 	MarkComponentsRenderStateDirty();
}

void AVLogRenderingActor::AddLogEntry(const FVisualLogEntry& Entry)
{
	GetDebugShapes(Entry, false, DebugShapes);
}

void AVLogRenderingActor::IterateDebugShapes(TFunction<void(const AVisualLoggerRenderingActorBase::FTimelineDebugShapes& Shapes)> Callback)
{
	Callback(DebugShapes);
}

