// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "UObject/Package.h"
#include "UObject/UObjectThreadContext.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ArchiveMD5.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "HAL/FileManager.h"
#include "Delegates/DelegateCombinations.h"

class ENGINE_API FExternalPackageHelper
{
public:

	DECLARE_EVENT_TwoParams(FExternalPackageHelper, FOnObjectPackagingModeChanged, UObject*, bool /* bExternal */);
	static FOnObjectPackagingModeChanged OnObjectPackagingModeChanged;

	/**
	 * Create an external package
	 * @param InObjectOuter the object's outer
	 * @param InObjectPath the fully qualified object path, in the format: 'Outermost.Outer.Name'
	 * @param InFlags the package flags to apply
	 * @return the created package
	 */
	static UPackage* CreateExternalPackage(UObject* InObjectOuter, const FString& InObjectPath, EPackageFlags InFlags);

	/**
	 * Set the object packaging mode.
	 * @param InObject the object on which to change the packaging mode
	 * @param InObjectOuter the object's outer
	 * @param bInIsPackageExternal will set the object packaging mode to external if true, to internal otherwise
	 * @param bInShouldDirty should dirty or not the object's outer package
	 * @param InExternalPackageFlags the flags to apply to the external package if bInIsPackageExternal is true
	 */
	static void SetPackagingMode(UObject* InObject, UObject* InObjectOuter, bool bInIsPackageExternal, bool bInShouldDirty, EPackageFlags InExternalPackageFlags);

	/**
	 * Get the path containing the external objects for this path
	 * @param InOuterPackageName The package name to get the external objects path of
	 * @param InPackageShortName Optional short name to use instead of the package short name
	 * @return the path
	 */
	static FString GetExternalObjectsPath(const FString& InOuterPackageName, const FString& InPackageShortName = FString());

	/**
	 * Get the path containing the external objects for this Outer
	 * @param InPackage The package to get the external objects path of
	 * @param InPackageShortName Optional short name to use instead of the package short name
	 * @return the path
	 */
	static FString GetExternalObjectsPath(UPackage* InPackage, const FString& InPackageShortName = FString(), bool bTryUsingPackageLoadedPath = false);


	/**
	 * Get the external package name for this object
	 * @param InOuterPackageName The name of the package of that contains the outer of the object.
	 * @param InObjectPath the fully qualified object path, in the format: 'Outermost.Outer.Name'
	 * @return the package name
	 */
	static FString GetExternalPackageName(const FString& InOuterPackageName, const FString& InObjectPath);

	/**
	 * Loads objects from an external package
	 */
	template<typename T>
	static void LoadObjectsFromExternalPackages(UObject* InOuter, TFunctionRef<void(T*)> Operation);

private:
	/** Get the external object package instance name. */
	static FString GetExternalObjectPackageInstanceName(const FString& OuterPackageName, const FString& ObjectPackageName);
};

static TArray<FString> GetOnDiskExternalObjectPackages(const FString& ExternalObjectsPath)
{
	TArray<FString> ObjectPackageNames;
	if (!ExternalObjectsPath.IsEmpty())
	{
		IFileManager::Get().IterateDirectoryRecursively(*FPackageName::LongPackageNameToFilename(ExternalObjectsPath), [&ObjectPackageNames](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (!bIsDirectory)
			{
				FString Filename(FilenameOrDirectory);
				if (Filename.EndsWith(FPackageName::GetAssetPackageExtension()))
				{
					ObjectPackageNames.Add(FPackageName::FilenameToLongPackageName(Filename));
				}
			}
			return true;
		});
	}
	return ObjectPackageNames;
}

template<typename T>
void FExternalPackageHelper::LoadObjectsFromExternalPackages(UObject* InOuter, TFunctionRef<void(T*)> Operation)
{
	const FString ExternalObjectsPath = FExternalPackageHelper::GetExternalObjectsPath(InOuter->GetPackage(), FString(), /*bTryUsingPackageLoadedPath*/ true);
	TArray<FString> ObjectPackageNames;

	// Test if function is called while inside postload
	if (!FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		// Do a synchronous scan of the world external objects path.			
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		AssetRegistry.ScanPathsSynchronous({ ExternalObjectsPath }, /*bForceRescan*/false, /*bIgnoreDenyListScanFilters*/false);

		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.bIncludeOnlyOnDiskAssets = true;
		Filter.ClassPaths.Add(T::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(*ExternalObjectsPath);

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets);

		ObjectPackageNames.Reserve(Assets.Num());
		for (const FAssetData& Asset : Assets)
		{
			ObjectPackageNames.Add(Asset.PackageName.ToString());
		}
	}
	// If so, we can't use AssetRegistry's ScanPathsSynchronous as it might update AssetRegistry object tags which can call BP events 
	// (which is not allowed while inside postload). Instead, get all external object packages on disk.
	// @todo_ow : This is far from ideal because once we move External Actors to share the same folder as external objects (or any new object we will externalize),
	//            we will discover and iterate on all external packages regarless of the class type we actually want to load.
	else
	{
		 ObjectPackageNames = GetOnDiskExternalObjectPackages(ExternalObjectsPath);
	}

	FLinkerInstancingContext InstancingContext;
	TArray<UPackage*> InstancePackages;

	UPackage* OuterPackage = InOuter->GetPackage();
	FName PackageResourceName = OuterPackage->GetLoadedPath().GetPackageFName();
	const bool bInstanced = !PackageResourceName.IsNone() && (PackageResourceName != OuterPackage->GetFName());
	if (bInstanced)
	{
		const FLinkerInstancingContext* OuterInstancingContext = nullptr;
		if (FLinkerLoad* OuterLinker = InOuter->GetLinker())
		{
			OuterInstancingContext = &OuterLinker->GetInstancingContext();
		}

		InstancingContext.AddPackageMapping(PackageResourceName, OuterPackage->GetFName());

		for (const FString& ObjectPackageName : ObjectPackageNames)
		{
			FName InstancedName;
			
			FName ObjectPackageFName = *ObjectPackageName;
			if (OuterInstancingContext)
			{
				InstancedName = OuterInstancingContext->RemapPackage(ObjectPackageFName);
			}

			// Remap to the a instanced package if it wasn't remapped already by the outer instancing context
			if (InstancedName == ObjectPackageFName || InstancedName.IsNone())
			{
				InstancedName = *GetExternalObjectPackageInstanceName(OuterPackage->GetName(), ObjectPackageName);
			}

			InstancingContext.AddPackageMapping(ObjectPackageFName, InstancedName);

			// Create instance package
			UPackage* InstancePackage = CreatePackage(*InstancedName.ToString());
			// Propagate RF_Transient
			if (OuterPackage->HasAnyFlags(RF_Transient))
			{
				InstancePackage->SetFlags(RF_Transient);
			}
			InstancePackages.Add(InstancePackage);
		}
	}

	const ELoadFlags LoadFlags = InOuter->GetPackage()->HasAnyPackageFlags(PKG_PlayInEditor) ? LOAD_PackageForPIE : LOAD_None;
	for (int32 i = 0; i < ObjectPackageNames.Num(); i++)
	{
		if (UPackage* Package = LoadPackage(bInstanced ? InstancePackages[i] : nullptr, *ObjectPackageNames[i], LoadFlags, nullptr, &InstancingContext))
		{
			T* LoadedObject = nullptr;
			ForEachObjectWithPackage(Package, [&LoadedObject](UObject* Object)
			{
				if (T* TypedObj = Cast<T>(Object))
				{
					LoadedObject = TypedObj;
					return false;
				}
				return true;
			}, true, RF_NoFlags, EInternalObjectFlags::Unreachable);

			if (ensure(LoadedObject))
			{
				Operation(LoadedObject);
			}
		}
	}
}

#endif