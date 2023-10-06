// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamOutputProviderBase.h"
#include "VCamOutputViewport.generated.h"

/** Adds a widget on top of the viewport locally. */
UCLASS(meta = (DisplayName = "Viewport Output Provider"))
class VCAMCORE_API UVCamOutputViewport : public UVCamOutputProviderBase
{
	GENERATED_BODY()
public:

	UVCamOutputViewport();
};
