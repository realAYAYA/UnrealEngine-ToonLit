// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreFwd.h"
#include "Containers/StringView.h"
#include "Templates/Function.h"

class FAssetRegistryTagsContext;
class UExternalDataLayerAsset;
struct FExternalDataLayerUID;
struct FAssetData;

class FExternalDataLayerHelper
{
public:
	/** Returns the external streaming object package name */
	static FString GetExternalStreamingObjectPackageName(const UExternalDataLayerAsset* InExternalDataLayerAsset);

	/** Returns the external streaming object name */
	static FString GetExternalStreamingObjectName(const UExternalDataLayerAsset* InExternalDataLayerAsset);

	/** Return true if succeeds building the external data layer root path (OutExternalDataLayerRootPath) using the provided mount point and EDL UID.
	 * Format is /{MountPoint}/{ExternalDataLayerFolder}/{EDL_UID}
	 */
	ENGINE_API static bool BuildExternalDataLayerRootPath(const FString& InEDLMountPoint, const FExternalDataLayerUID& InExternalDataLayerUID, FString& OutExternalDataLayerRootPath);

	/** Return true if succeeds building the external data layer level root path (OutExternalDataLayerLevelRootPath) using the ExternalDataLayerAsset and the level package path.
	  * Format is /{MountPoint}/{ExternalDataLayerFolder}/{EDL_UID}/{LevelPath}
	  */
	ENGINE_API static FString GetExternalDataLayerLevelRootPath(const UExternalDataLayerAsset* InExternalDataLayerAsset, const FString& InLevelPackagePath);

	/** Return true if succeeds building the external data layer level root path (OutExternalDataLayerLevelRootPath) using the provided mount point, EDL UID and the level package path.
	  * Format is /{MountPoint}/{ExternalDataLayerFolder}/{EDL_UID}/{LevelPath}
	  */
	ENGINE_API static FString GetExternalDataLayerLevelRootPath(const FString& InEDLMountPoint, const FExternalDataLayerUID& InExternalDataLayerUID, const FString& InLevelPackagePath);

#if WITH_EDITOR
	/** Returns whether the provided path respects the format <start_path>/{ExternalDataLayerFolder}/{EDL_UID}/<end_path>. 
	 * If true, fills OutExternalDataLayerUID when provided.
	 */
	ENGINE_API static bool IsExternalDataLayerPath(FStringView InExternalDataLayerPath, FExternalDataLayerUID* OutExternalDataLayerUID = nullptr);

	/** Fills Asset Registry Tags Context with provided External Data Layer UIDs. */
	ENGINE_API static void AddAssetRegistryTags(FAssetRegistryTagsContext OutContext, const TArray<FExternalDataLayerUID>& InExternalDataLayerUIDs);

	/** Retrieves External Data Layers UIDs from provided Asset. */
	ENGINE_API static void GetExternalDataLayerUIDs(const FAssetData& Asset, TArray<FExternalDataLayerUID>& OutExternalDataLayerUIDs);

	/** Iterates through all possible External Data Layer Level Package Paths using Asset Registry. */
	ENGINE_API static void ForEachExternalDataLayerLevelPackagePath(const FString& InLevelPackageName, TFunctionRef<void(const FString&)> Func);
#endif

private:

	static constexpr FStringView GetExternalDataLayerFolder() { return ExternalDataLayerFolder; }
	static constexpr FStringView ExternalDataLayerFolder = TEXTVIEW("/EDL/");
};