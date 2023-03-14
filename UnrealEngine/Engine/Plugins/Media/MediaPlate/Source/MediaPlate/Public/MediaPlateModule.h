// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

class AMediaPlate;
class IMediaPlayerProxy;
class UMaterialInterface;
class UMediaPlayer;
class UObject;

/** Log category for this module. */
DECLARE_LOG_CATEGORY_EXTERN(LogMediaPlate, Log, All);

/**
 * Callback when a media plate applies a material to itself.
 * Set bCanModify to false if you do not want the media plate to modify the material so it can show
 * the media plate's texture.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMediaPlateApplyMaterial, UMaterialInterface*, AMediaPlate*, bool& /*bCanNodify*/)

class MEDIAPLATE_API FMediaPlateModule : public IModuleInterface
{
public:
	/**
	 * Call this to get the media player from a media plate object.
	 */
	UMediaPlayer* GetMediaPlayer(UObject* Object, UObject*& PlayerProxy);

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Add to this if you want a callback when a media plate applies a material to itself. */
	FOnMediaPlateApplyMaterial OnMediaPlateApplyMaterial;

private:
	/** ID for our delegate. */
	int32 GetPlayerFromObjectID = INDEX_NONE;
};
