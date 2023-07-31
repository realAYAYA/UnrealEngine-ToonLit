// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureStats.h"

#include "UObject/WeakObjectPtr.h"

#include <cfloat>

UTextureStats::UTextureStats(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	LastTimeRendered( FLT_MAX )
{
}
