// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHITexture;
class FRHICommandListImmediate;

/** Interface for a manager which manages the rendering of UV light cards for the viewport manager */
class DISPLAYCLUSTER_API IDisplayClusterViewportLightCardManager
{
public:
	/** Gets the render thread copy of the UV light card map texture */
	virtual FRHITexture* GetUVLightCardMap_RenderThread() const = 0;
};