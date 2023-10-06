// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Lightweight struct used to list the MIP levels of rendered assets.
 */
struct FRenderedTextureStats
{
	int32 MaxMipLevelShown;
	FString TextureGroup;
};
