// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleInterface.h"

class FPlacementModeID;
class IAssetTypeActions;

struct FPlacementCategoryInfo;

/**
 * Implements the CameraCalibrationEditor module.
 */
class FCameraCalibrationEditorModule : public IModuleInterface
{
public:

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:

	/** Register items to show up in the Place Actors panel. */
	void RegisterPlacementModeItems();

	/** Unregister items in Place Actors panel */
	void UnregisterPlacementModeItems();

	/** Register overlay materials to use in the calibration tool */
	void RegisterOverlayMaterials();

	/** Unregister overlay materials */
	void UnregisterOverlayMaterials();

	/** Gathers the Info on the Virtual Production Place Actors Category */
	const FPlacementCategoryInfo* GetVirtualProductionCategoryRegisteredInfo() const;

private:

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
	FDelegateHandle PostEngineInitHandle;

	TArray<TOptional<FPlacementModeID>> PlaceActors;
};
