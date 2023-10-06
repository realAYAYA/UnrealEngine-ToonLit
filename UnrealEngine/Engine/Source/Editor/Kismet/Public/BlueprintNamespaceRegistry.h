// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Templates/UniquePtr.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

class FName;
class UObject;
struct FAssetData;
struct FBlueprintNamespacePathTree;

/**
 * A shared utility class that keeps track of registered Blueprint namespace identifiers sourced from objects and assets in the editor.
 */
class KISMET_API FBlueprintNamespaceRegistry
{
public:
	~FBlueprintNamespaceRegistry();

	/**
	 * Provides public singleton access.
	 */
	static FBlueprintNamespaceRegistry& Get();

	/**
	 * One-time initialization method; separated from the ctor so it can be called explicitly.
	 */
	void Initialize();

	/**
	 * One-time shutdown method; separated from the dtor so it can be called explicitly.
	 */
	void Shutdown();

	/**
	 * @return TRUE if the given path identifier is currently registered.
	 */
	bool IsRegisteredPath(const FString& InPath) const;
	
	/**
	 * @return TRUE if the given path identifier is inclusive of any registered paths.
	 * 
	 * Example: If "MyProject.MyNamespace" is a registered path, then both "MyProject" and "MyProject.MyNamespace" are inclusive paths.
	 * 
	 * Also note if a registered path is removed, inclusive paths may still be valid. For instance, if both "MyProject.MyNamespace" and
	 * "MyProject.MyNamespace_2" are registered paths, and "MyProject.MyNamespace_2" is removed, "MyProject" is still an inclusive path.
	 */
	bool IsInclusivePath(const FString& InPath) const;

	/**
	 * @param InPath	Path identifier string (e.g. "X.Y" or "X.Y.").
	 * @param OutNames	On output, an array containing the set of names rooted to the given path (e.g. "Z" in "X.Y.Z").
	 */
	void GetNamesUnderPath(const FString& InPath, TArray<FName>& OutNames) const;

	/**
	 * @param OutPaths	On output, contains the full set of all currently-registered namespace identifier paths.
	 */
	void GetAllRegisteredPaths(TArray<FString>& OutPaths) const;

	/**
	 * Adds an explicit namespace identifier to the registry if not already included.
	 * 
	 * @param InPath	Path identifier string (e.g. "X.Y").
	 */
	void RegisterNamespace(const FString& InPath);

	/**
	 * Recreates the namespace registry.
	 */
	void Rebuild();

protected:
	FBlueprintNamespaceRegistry();

	/** Asset registry event handler methods. */
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnAssetRenamed(const FAssetData& AssetData, const FString& InOldName);
	void OnAssetRegistryFilesLoaded();

	/** Namespace identifier registration methods. */
	void FindAndRegisterAllNamespaces();
	void RegisterNamespace(const UObject* InObject);
	void RegisterNamespace(const FAssetData& AssetData);

	/** Console command implementations (debugging/testing). */
	void ToggleDefaultNamespace();
	void DumpAllRegisteredPaths();
	void OnDefaultNamespaceTypeChanged();

	/** Handler for hot reload / live coding completion events. */
	void OnReloadComplete(EReloadCompleteReason InReason);

private:
	/** Indicates whether the registry has been initialized. */
	bool bIsInitialized;

	/** Delegate handles to allow for deregistration on shutdown. */
	FDelegateHandle OnAssetAddedDelegateHandle;
	FDelegateHandle OnAssetRemovedDelegateHandle;
	FDelegateHandle OnAssetRenamedDelegateHandle;
	FDelegateHandle OnFilesLoadedDelegateHandle;
	FDelegateHandle OnReloadCompleteDelegateHandle;
	FDelegateHandle OnDefaultNamespaceTypeChangedDelegateHandle;

	/** Handles storage and retrieval for namespace path identifiers. */
	TUniquePtr<FBlueprintNamespacePathTree> PathTree;

	/** Internal set of objects to exclude during namespace registration. */
	TSet<FSoftObjectPath> ExcludedObjectPaths;
};