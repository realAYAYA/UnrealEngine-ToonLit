// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDMXPixelMappingRendererModule.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class IDMXPixelMappingRenderer;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

class FDMXPixelMappingRendererModule 
	: public IDMXPixelMappingRendererModule
{
public:

	//~ Begin IModuleInterface implementation
	virtual void StartupModule() override;
	//~ End IModuleInterface implementation

	//~ Begin IDMXPixelMappingRendererModule implementation
	UE_DEPRECATED(5.3, "IDMXPixelMappingRenderer was replaced with UDMXPixelMappigPixelMapRenderer")
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual TSharedPtr<IDMXPixelMappingRenderer> CreateRenderer() const override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	//~ End IDMXPixelMappingRendererModule implementation
};
