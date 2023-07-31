// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"

class UCustomizableObject;

// Public interface to Customizable Object Editor
class ICustomizableObjectDebugger : public FAssetEditorToolkit
{
public:
	// Retrieves the current Customizable Object displayed in the Editor.
	virtual UCustomizableObject* GetCustomizableObject() = 0;

};


