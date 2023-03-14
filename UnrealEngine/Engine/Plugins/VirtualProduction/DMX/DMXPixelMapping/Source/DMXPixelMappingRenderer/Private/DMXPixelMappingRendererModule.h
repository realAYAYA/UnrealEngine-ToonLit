// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDMXPixelMappingRendererModule.h"

class IDMXPixelMappingRenderer;

class FDMXPixelMappingRendererModule 
	: public IDMXPixelMappingRendererModule
{
public:

	//~ Begin IModuleInterface implementation
	virtual void StartupModule() override;
	//~ End IModuleInterface implementation

	//~ Begin IDMXPixelMappingRendererModule implementation
	virtual TSharedPtr<IDMXPixelMappingRenderer> CreateRenderer() const override;
	//~ End IDMXPixelMappingRendererModule implementation
};
