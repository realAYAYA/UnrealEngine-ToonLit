// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClient.h"

class SDMXPixelMappingSourceTextureViewport;
class FDMXPixelMappingToolkit;

class FDMXPixelMappingSourceTextureViewportClient
	: public FViewportClient
{
public:
	/** Constructor */
	FDMXPixelMappingSourceTextureViewportClient(const TSharedPtr<FDMXPixelMappingToolkit>& Toolkit, TWeakPtr<SDMXPixelMappingSourceTextureViewport> InViewport);
	
	//~ Begin FViewportClient Interface
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	//~ End FViewportClient Interface

private:
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;

	TWeakPtr<SDMXPixelMappingSourceTextureViewport> WeakViewport;
};
