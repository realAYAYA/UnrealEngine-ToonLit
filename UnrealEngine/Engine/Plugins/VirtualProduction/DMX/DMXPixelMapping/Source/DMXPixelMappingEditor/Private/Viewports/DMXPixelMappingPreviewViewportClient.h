// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClient.h"

class SDMXPixelMappingPreviewViewport;
class FDMXPixelMappingToolkit;

class FDMXPixelMappingPreviewViewportClient
	: public FViewportClient
{
public:
	/** Constructor */
	FDMXPixelMappingPreviewViewportClient(const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit, TWeakPtr<SDMXPixelMappingPreviewViewport> InViewport);

	//~ Begin FViewportClient Interface
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	//~ End FViewportClient Interface

private:
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;
	TWeakPtr<SDMXPixelMappingPreviewViewport> WeakViewport;
};
