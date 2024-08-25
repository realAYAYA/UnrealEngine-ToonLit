// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class UUsdAssetCache2;

enum class EDefaultAssetCacheDialogOption : uint8
{
	Cancel = 0,
	PickExisting,
	CreateNew,
	DontUseDefault
};

class IUsdClassesEditorModule : public IModuleInterface
{
public:
	/** Shows the dialog that asks the user to set a default asset cache for the project */
	UE_DEPRECATED(5.3, "Use the other signature that also returns whether the user accepted the dialog or not")
	USDCLASSESEDITOR_API static UUsdAssetCache2* ShowMissingDefaultAssetCacheDialog();

	UE_DEPRECATED(5.4, "Use the other signature that also returns the dialog outcome as an Enum")
	USDCLASSESEDITOR_API static void ShowMissingDefaultAssetCacheDialog(UUsdAssetCache2*& OutCreatedCache, bool& bOutUserAccepted);

	USDCLASSESEDITOR_API static EDefaultAssetCacheDialogOption ShowMissingDefaultAssetCacheDialog(UUsdAssetCache2*& OutCreatedCache);
};
