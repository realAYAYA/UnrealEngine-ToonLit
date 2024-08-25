// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVGEngineSubsystem.h"
#include "Engine/Engine.h"

FOnSVGActorSplit USVGEngineSubsystem::OnSVGActorSplitDelegate;

USVGEngineSubsystem* USVGEngineSubsystem::Get()
{
	if (GEngine)
	{
		return GEngine->GetEngineSubsystem<USVGEngineSubsystem>();
	}
	return nullptr;
}
