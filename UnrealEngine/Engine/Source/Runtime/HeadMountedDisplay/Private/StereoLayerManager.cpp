// Copyright Epic Games, Inc. All Rights Reserved.

#include "StereoLayerManager.h"

bool GetLayerDescMember(const IStereoLayers::FLayerDesc& Layer, IStereoLayers::FLayerDesc& OutLayerDesc)
{
	OutLayerDesc = Layer;
	return true;
}

void SetLayerDescMember(IStereoLayers::FLayerDesc& OutLayer, const IStereoLayers::FLayerDesc& InLayerDesc)
{
	OutLayer = InLayerDesc;
}

void MarkLayerTextureForUpdate(IStereoLayers::FLayerDesc& Layer)
{
	// Not used
}
