// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareDisplayCluster.h"
#include "Module/TextureShareDisplayClusterAPI.h"

/**
 * Module Impl
 */
class FTextureShareDisplayCluster
	: public ITextureShareDisplayCluster
{
public:
	FTextureShareDisplayCluster();
	virtual ~FTextureShareDisplayCluster();

public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// ~IModuleInterface

	virtual ITextureShareDisplayClusterAPI& GetTextureShareDisplayClusterAPI() override;

protected:
	TUniquePtr<FTextureShareDisplayClusterAPI> TextureShareDisplayClusterAPI;
};
