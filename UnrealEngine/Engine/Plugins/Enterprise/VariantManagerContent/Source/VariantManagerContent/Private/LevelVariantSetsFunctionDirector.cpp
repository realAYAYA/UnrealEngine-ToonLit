// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelVariantSetsFunctionDirector.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelVariantSetsFunctionDirector)

UWorld* ULevelVariantSetsFunctionDirector::GetWorld() const
{
	return GetTypedOuter<UWorld>();
}
