// Copyright Epic Games, Inc. All Rights Reserved.

#include "SurfaceEffectsSubsystem.h"

#include "SurfaceEffectsSettings.h"

void USurfaceEffectsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const USurfaceEffectsSettings* MutableDefault = GetMutableDefault<USurfaceEffectsSettings>();

	if(MutableDefault)
	{
		SurfaceEffectsData = LoadObject<UDataTable>(nullptr, *MutableDefault->SurfaceEffectsDataTable.ToString(), nullptr, LOAD_None, nullptr);
	}
}
