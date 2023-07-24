// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class UUsdAssetCache2;

class IUsdClassesEditorModule : public IModuleInterface
{
public:
    /** Shows the dialog that asks the user to set a default asset cache for the project */
	USDCLASSESEDITOR_API static UUsdAssetCache2* ShowMissingDefaultAssetCacheDialog();
};