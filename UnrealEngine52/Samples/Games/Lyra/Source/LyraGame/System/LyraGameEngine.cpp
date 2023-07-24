// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameEngine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameEngine)

class IEngineLoop;


ULyraGameEngine::ULyraGameEngine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULyraGameEngine::Init(IEngineLoop* InEngineLoop)
{
	Super::Init(InEngineLoop);
}

