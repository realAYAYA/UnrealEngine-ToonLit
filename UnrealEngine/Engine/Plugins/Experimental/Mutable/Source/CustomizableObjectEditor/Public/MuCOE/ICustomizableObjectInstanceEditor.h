// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"

class UCustomizableObject;
class UCustomizableObjectInstance;

/**
 * Public interface to Customizable Object Instance Editor
 */
class ICustomizableObjectInstanceEditor : public FAssetEditorToolkit
{
public:
	virtual UCustomizableObjectInstance* GetPreviewInstance() = 0;

	/** Refreshes the Customizable Object Instance Editor's viewport. */
	virtual void RefreshViewport() = 0;

	/** Refreshes everything in the Customizable Object Instance Editor. */
	virtual void RefreshTool() = 0;

	/** Getter of AssetRegistryLoaded */
	virtual bool GetAssetRegistryLoaded() = 0;

	virtual void SetPoseAsset(class UPoseAsset* PoseAssetParameter) {};
};


