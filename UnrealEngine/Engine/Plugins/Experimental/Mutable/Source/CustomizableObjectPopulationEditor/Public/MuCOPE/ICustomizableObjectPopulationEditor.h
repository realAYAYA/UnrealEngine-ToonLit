// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"
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

