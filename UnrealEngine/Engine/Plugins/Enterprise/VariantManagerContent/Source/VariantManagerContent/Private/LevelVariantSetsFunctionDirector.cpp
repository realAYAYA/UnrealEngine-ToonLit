// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelVariantSetsFunctionDirector.h"
#include "Engine/World.h"

UWorld* ULevelVariantSetsFunctionDirector::GetWorld() const
{
	return GetTypedOuter<UWorld>();
}
