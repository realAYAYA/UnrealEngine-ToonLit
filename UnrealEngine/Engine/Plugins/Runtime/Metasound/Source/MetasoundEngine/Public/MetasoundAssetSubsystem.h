// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/Object.h"

#include "MetasoundAssetSubsystem.generated.h"

// Forward Declarations
class UAssetManager;
class FMetasoundAssetBase;

struct FDirectoryPath;


USTRUCT(BlueprintType)
struct METASOUNDENGINE_API FMetaSoundAssetDirectory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Directories, meta = (RelativePath, LongPackageName))
	FDirectoryPath Directory;
};

/** Contains info of assets which are currently async loading. */
USTRUCT()
struct FMetaSoundAsyncAssetDependencies
{
	GENERATED_BODY()

	// ID of the async load
	int32 LoadID = 0;

	// Parent MetaSound 
	UPROPERTY(Transient)
	TObjectPtr<UObject> MetaSound;

	// Dependencies of parent MetaSound
	TArray<FSoftObjectPath> Dependencies;

	// Handle to in-flight streaming request
	TSharedPtr<FStreamableHandle> StreamableHandle;
};

/** The subsystem in charge of the MetaSound asset registry */
UCLASS()
class METASOUNDENGINE_API UMetaSoundAssetSubsystem : public UEngineSubsystem, public Metasound::Frontend::IMetaSoundAssetManager
{
	GENERATED_BODY()

public:
	using FAssetInfo = Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo;

	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;

	Metasound::Frontend::FNodeRegistryKey AddOrUpdateAsset(const FAssetData& InAssetData);
	void RemoveAsset(const UObject& InObject);
	void RemoveAsset(const FAssetData& InAssetData);
	void RenameAsset(const FAssetData& InAssetData, bool bInReregisterWithFrontend = true);

#if WITH_EDITORONLY_DATA
	virtual void AddAssetReferences(FMetasoundAssetBase& InAssetBase) override;
#endif
	virtual Metasound::Frontend::FNodeRegistryKey  AddOrUpdateAsset(const UObject& InObject) override;
	virtual bool CanAutoUpdate(const FMetasoundFrontendClassName& InClassName) const override;
	virtual bool ContainsKey(const Metasound::Frontend::FNodeRegistryKey& InRegistryKey) const override;
	virtual const FSoftObjectPath* FindObjectPathFromKey(const Metasound::Frontend::FNodeRegistryKey& RegistryKey) const override;
#if WITH_EDITOR
	virtual TSet<FAssetInfo> GetReferencedAssetClasses(const FMetasoundAssetBase& InAssetBase) const override;
#endif
	virtual void RescanAutoUpdateDenyList() override;
	virtual FMetasoundAssetBase* TryLoadAsset(const FSoftObjectPath& InObjectPath) const override;
	virtual FMetasoundAssetBase* TryLoadAssetFromKey(const Metasound::Frontend::FNodeRegistryKey& RegistryKey) const override;
	virtual bool TryLoadReferencedAssets(const FMetasoundAssetBase& InAssetBase, TArray<FMetasoundAssetBase*>& OutReferencedAssets) const override;
	virtual void RequestAsyncLoadReferencedAssets(FMetasoundAssetBase& InAssetBase) override;
	virtual void WaitUntilAsyncLoadReferencedAssetsComplete(FMetasoundAssetBase& InAssetBase) override;

	UFUNCTION(BlueprintCallable, Category = "MetaSounds|Registration")
	void RegisterAssetClassesInDirectories(const TArray<FMetaSoundAssetDirectory>& Directories);

	UFUNCTION(BlueprintCallable, Category = "MetaSounds|Registration")
	void UnregisterAssetClassesInDirectories(const TArray<FMetaSoundAssetDirectory>& Directories);

protected:

	void PostEngineInit();
	void PostInitAssetScan();
	void RebuildDenyListCache(const UAssetManager& InAssetManager);
	void ResetAssetClassDisplayName(const FAssetData& InAssetData);
	void SearchAndIterateDirectoryAssets(const TArray<FDirectoryPath>& InDirectories, TFunctionRef<void(const FAssetData&)> InFunction);

private:

	UPROPERTY(Transient)
	TArray<FMetaSoundAsyncAssetDependencies> LoadingDependencies;

	FMetaSoundAsyncAssetDependencies* FindLoadingDependencies(const UObject* InParentAsset);
	FMetaSoundAsyncAssetDependencies* FindLoadingDependencies(int32 InLoadID);
	void RemoveLoadingDependencies(int32 InLoadID);
	void OnAssetsLoaded(int32 InLoadID);

	FStreamableManager StreamableManager;
	int32 AsyncLoadIDCounter = 0;
	int32 AutoUpdateDenyListChangeID = INDEX_NONE;
	TSet<FName> AutoUpdateDenyListCache;
	TMap<Metasound::Frontend::FNodeRegistryKey, FSoftObjectPath> PathMap;
	std::atomic<bool> bIsInitialAssetScanComplete = false;
};
