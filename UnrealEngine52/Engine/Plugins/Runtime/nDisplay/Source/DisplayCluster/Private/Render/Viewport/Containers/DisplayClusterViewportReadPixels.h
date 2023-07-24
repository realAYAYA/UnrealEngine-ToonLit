// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDisplayClusterViewportReadPixelsData
{
public:
	FIntPoint Size;
	TArray<FColor> Pixels;
};
