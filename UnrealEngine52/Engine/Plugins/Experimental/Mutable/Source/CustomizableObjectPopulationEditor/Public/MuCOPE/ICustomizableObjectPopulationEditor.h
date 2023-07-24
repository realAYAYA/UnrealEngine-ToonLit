// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorToolkit.h"

class UCustomizableObjectPopulation;

/**
 * Public interface to Static Mesh Editor
 */
class ICustomizableObjectPopulationEditor : public FAssetEditorToolkit
{
public:
	/** Retrieves the current custom asset. */
	virtual UCustomizableObjectPopulation* GetCustomAsset() = 0;

	/** Set the current custom asset. */
	virtual void SetCustomAsset(UCustomizableObjectPopulation* InCustomAsset) {};
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Toolkits/IToolkitHost.h"
#endif
