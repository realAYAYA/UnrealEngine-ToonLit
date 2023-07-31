// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"

class UCustomizableObjectPopulationClass;

/**
 * Public interface to Static Mesh Editor
 */
class ICustomizableObjectPopulationClassEditor : public FAssetEditorToolkit
{
public:
	/** Retrieves the current custom asset. */
	virtual UCustomizableObjectPopulationClass* GetCustomAsset() = 0;

	/** Set the current custom asset. */
	virtual void SetCustomAsset(UCustomizableObjectPopulationClass* InCustomAsset) {};
};

