// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Misc/AssetRegistryInterface.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

class UPackage;

namespace AssetRegistryConstants
{
	const FName ModuleName("AssetRegistry");
}

/**
 * Asset registry module
 */
class FAssetRegistryModule : public IModuleInterface, public IAssetRegistryInterface
{

public:

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override;

	virtual IAssetRegistry& Get() const
	{
		return IAssetRegistry::GetChecked();
	}

	/** Reports whether Get is valid to call. Will be true except during engine shutdown. */
	bool IsValid() const
	{
		return IAssetRegistry::Get() != nullptr;
	}

	/** Returns AssetRegistry pointer if valid, or nullptr. Will be non-null except during engine shutdown. */
	IAssetRegistry* TryGet() const
	{
		return IAssetRegistry::Get();
	}

	static IAssetRegistry& GetRegistry()
	{
		return IAssetRegistry::GetChecked();
	}

	static void TickAssetRegistry(float DeltaTime)
	{
		IAssetRegistry::GetChecked().Tick(DeltaTime);
	}

	static void AssetCreated(UObject* NewAsset)
	{
		IAssetRegistry::GetChecked().AssetCreated(NewAsset);
	}

	static void AssetDeleted(UObject* DeletedAsset)
	{
		IAssetRegistry::GetChecked().AssetDeleted(DeletedAsset);
	}

	static void AssetRenamed(const UObject* RenamedAsset, const FString& OldObjectPath)
	{
		IAssetRegistry::GetChecked().AssetRenamed(RenamedAsset, OldObjectPath);
	}

	UE_DEPRECATED(5.2, "Use the new AssetsSaved function that takes FAssetData.")
	static void AssetSaved(const UObject& SavedAsset)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS;
		IAssetRegistry::GetChecked().AssetSaved(SavedAsset);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS;
	}

	static void AssetsSaved(TArray<FAssetData>&& SavedAssets)
	{
		IAssetRegistry::GetChecked().AssetsSaved(MoveTemp(SavedAssets));
	}

	static void PackageDeleted(UPackage* DeletedPackage)
	{
		IAssetRegistry::GetChecked().PackageDeleted(DeletedPackage);
	}

	/** Access the dependent package names for a given source package */
	virtual void GetDependencies(FName InPackageName, TArray<FName>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::Package, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) override
	{
		IAssetRegistry::GetChecked().GetDependencies(InPackageName, OutDependencies, Category, Flags);
	}

	virtual UE::AssetRegistry::EExists TryGetAssetByObjectPath(const FSoftObjectPath& ObjectPath, FAssetData& OutAssetData) const override
	{
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry)
		{
			return UE::AssetRegistry::EExists::Unknown;
		}
		return AssetRegistry->TryGetAssetByObjectPath(ObjectPath, OutAssetData);
	}

	virtual UE::AssetRegistry::EExists TryGetAssetPackageData(FName PackageName, FAssetPackageData& OutAssetPackageData) const override
	{
		IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry)
		{
			return UE::AssetRegistry::EExists::Unknown;
		}
		return AssetRegistry->TryGetAssetPackageData(PackageName, OutAssetPackageData);
	}
};
