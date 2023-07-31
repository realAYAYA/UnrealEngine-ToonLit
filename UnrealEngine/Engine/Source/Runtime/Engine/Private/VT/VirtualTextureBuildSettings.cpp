// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT/VirtualTextureBuildSettings.h"

#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(VirtualTextureBuildSettings)

static TAutoConsoleVariable<int32> CVarVTTileSize(
	TEXT("r.VT.TileSize"),
	128,
	TEXT("Size in pixels to use for virtual texture tiles (rounded to next power-of-2)")
);

static TAutoConsoleVariable<int32> CVarVTTileBorderSize(
	TEXT("r.VT.TileBorderSize"),
	4,
	TEXT("Size in pixels to use for virtual texture tiles borders (rounded to next power-of-2)")
);

void FVirtualTextureBuildSettings::Init()
{
	TileSize = CVarVTTileSize.GetValueOnAnyThread();
	TileBorderSize = CVarVTTileBorderSize.GetValueOnAnyThread();
}

