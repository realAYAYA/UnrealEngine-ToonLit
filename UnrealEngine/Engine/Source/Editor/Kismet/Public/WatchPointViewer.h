// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Misc/CoreMiscDefines.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FTabManager;
class UBlueprint;
struct FAssetData;

#define WATCH_VIEWER_DEPRECATED

#ifdef WATCH_VIEWER_DEPRECATED
namespace UE_DEPRECATED(5.0, "WatchViewer has been deprecated, use SKismetDebuggingView instead") WatchViewer
#else
namespace WatchViewer
#endif
{
	// updates the instanced watch values, these are only valid while execution is paused
	void KISMET_API UpdateInstancedWatchDisplay();

	// called when we unpause execution and set watch values back to the blueprint versions
	void KISMET_API ContinueExecution();

	// called when we are adding or changing watches from BlueprintObj
	void KISMET_API UpdateWatchListFromBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj);

	// called when we want to remove watches in the watch window from a blueprint
	// does NOT remove watches from the pins in the blueprint object
	void KISMET_API RemoveWatchesForBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj);

	// called when we want to remove watches in the watch window from a blueprint
	// does NOT remove watches from the pins in the blueprint object
	void KISMET_API RemoveWatchesForAsset(const FAssetData& AssetData);

	// called when an asset is renamed; updates the watches on the asset
	void KISMET_API OnRenameAsset(const FAssetData& AssetData, const FString& OldAssetName);

	// called when a BlueprintObj should no longer be watched
	void KISMET_API ClearWatchListFromBlueprint(TWeakObjectPtr<UBlueprint> BlueprintObj);

	FName GetTabName();
	void RegisterTabSpawner(FTabManager& TabManager);
}
