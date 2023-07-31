// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundAssetSubsystem.h"

#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Metasound.h"
#include "MetasoundAssetBase.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundSettings.h"
#include "MetasoundSource.h"
#include "MetasoundTrace.h"
#include "MetasoundUObjectRegistry.h"
#include "UObject/NoExportTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundAssetSubsystem)


namespace Metasound
{
	namespace AssetSubsystemPrivate
	{
		bool GetAssetClassInfo(const FAssetData& InAssetData, Frontend::FNodeClassInfo& OutInfo)
		{
			using namespace Metasound;
			using namespace Metasound::Frontend;

			bool bSuccess = true;

			OutInfo.Type = EMetasoundFrontendClassType::External;
			OutInfo.AssetPath = InAssetData.GetSoftObjectPath();
			FString AssetClassID;
			bSuccess &= InAssetData.GetTagValue(AssetTags::AssetClassID, AssetClassID);
			OutInfo.AssetClassID = FGuid(AssetClassID);
			OutInfo.ClassName = FMetasoundFrontendClassName(FName(), *AssetClassID, FName());

#if WITH_EDITORONLY_DATA
			InAssetData.GetTagValue(AssetTags::IsPreset, OutInfo.bIsPreset);
#endif // WITH_EDITORONLY_DATA

			int32 RegistryVersionMajor = 0;
			bSuccess &= InAssetData.GetTagValue(AssetTags::RegistryVersionMajor, RegistryVersionMajor);
			OutInfo.Version.Major = RegistryVersionMajor;

			int32 RegistryVersionMinor = 0;
			bSuccess &= InAssetData.GetTagValue(AssetTags::RegistryVersionMinor, RegistryVersionMinor);
			OutInfo.Version.Minor = RegistryVersionMinor;

#if WITH_EDITORONLY_DATA
			auto ParseTypesString = [&](const FName AssetTag, TSet<FName>& OutTypes)
			{
				FString TypesString;
				if (InAssetData.GetTagValue(AssetTag, TypesString))
				{
					TArray<FString> DataTypeStrings;
					TypesString.ParseIntoArray(DataTypeStrings, *AssetTags::ArrayDelim);
					Algo::Transform(DataTypeStrings, OutTypes, [](const FString& DataType) { return *DataType; });
					return true;
				}

				return false;
			};

			// These values are optional and not necessary to return successfully as MetaSounds
			// don't require inputs or outputs for asset tags to be valid (ex. a new MetaSound,
			// non-source asset has no inputs or outputs)
			OutInfo.InputTypes.Reset();
			ParseTypesString(AssetTags::RegistryInputTypes, OutInfo.InputTypes);

			OutInfo.OutputTypes.Reset();
			ParseTypesString(AssetTags::RegistryOutputTypes, OutInfo.OutputTypes);
#endif // WITH_EDITORONLY_DATA

			return bSuccess;
		}

		// Remove the Map entry only if the key and value are equal.
		//
		// This protects against scenarios where a metasound is renamed or moved 
		// and the new entry was being erroneously removed from the PathMap
		bool RemoveIfExactMatch(TMap<Frontend::FNodeRegistryKey, FSoftObjectPath>& InMap, const Frontend::FNodeRegistryKey& InKeyToRemove, const FSoftObjectPath& InPathToRemove)
		{
			if (const FSoftObjectPath* Path = InMap.Find(InKeyToRemove))
			{
				if (*Path == InPathToRemove)
				{
					InMap.Remove(InKeyToRemove);
					return true;
				}
				else
				{
					UE_LOG(LogMetaSound, VeryVerbose, TEXT("Object paths do not match. Skipping removal of %s:%s from the asset subsystem."), *InKeyToRemove, *InPathToRemove.ToString());
				}
			}
			return false;
		}
	}
}

void UMetaSoundAssetSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	using namespace Metasound::Frontend;

	IMetaSoundAssetManager::Set(*this);
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UMetaSoundAssetSubsystem::PostEngineInit);
}

void UMetaSoundAssetSubsystem::PostEngineInit()
{
	if (UAssetManager* AssetManager = UAssetManager::GetIfValid())
	{
		AssetManager->CallOrRegister_OnCompletedInitialScan(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &UMetaSoundAssetSubsystem::PostInitAssetScan));
		RebuildDenyListCache(*AssetManager);
	}
	else
	{
		UE_LOG(LogMetaSound, Error, TEXT("Cannot initialize MetaSoundAssetSubsystem: Enable AssetManager or disable MetaSound plugin"));
	}
}

void UMetaSoundAssetSubsystem::PostInitAssetScan()
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundAssetSubsystem::PostInitAssetScan);

	const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
	if (ensureAlways(Settings))
	{
		SearchAndIterateDirectoryAssets(Settings->DirectoriesToRegister, [this](const FAssetData& AssetData)
		{
			AddOrUpdateAsset(AssetData);
		});
	}

	bIsInitialAssetScanComplete = true;
}

#if WITH_EDITORONLY_DATA
void UMetaSoundAssetSubsystem::AddAssetReferences(FMetasoundAssetBase& InAssetBase)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	const FNodeClassInfo AssetClassInfo = InAssetBase.GetAssetClassInfo();
	const FNodeRegistryKey AssetClassKey = NodeRegistryKey::CreateKey(AssetClassInfo);

	if (!ContainsKey(AssetClassKey))
	{
		AddOrUpdateAsset(*InAssetBase.GetOwningAsset());
		UE_LOG(LogMetaSound, Verbose, TEXT("Adding asset '%s' to MetaSoundAsset registry."), *InAssetBase.GetOwningAssetName());
	}

	bool bAddFromReferencedAssets = false;
	const TSet<FString>& ReferencedAssetClassKeys = InAssetBase.GetReferencedAssetClassKeys();
	for (const FString& ReferencedAssetClassKey : ReferencedAssetClassKeys)
	{
		if (!ContainsKey(ReferencedAssetClassKey))
		{
			UE_LOG(LogMetaSound, Verbose, TEXT("Missing referenced class '%s' asset entry."), *ReferencedAssetClassKey);
			bAddFromReferencedAssets = true;
		}
	}

	// All keys are loaded
	if (!bAddFromReferencedAssets)
	{
		return;
	}

	UE_LOG(LogMetaSound, Verbose, TEXT("Attempting preemptive reference load..."));

	TArray<FMetasoundAssetBase*> ReferencedAssets = InAssetBase.GetReferencedAssets();
	for (FMetasoundAssetBase* Asset : ReferencedAssets)
	{
		if (Asset)
		{
			FNodeClassInfo ClassInfo = Asset->GetAssetClassInfo();
			const FNodeRegistryKey ClassKey = NodeRegistryKey::CreateKey(ClassInfo);
			if (!ContainsKey(ClassKey))
			{
				UE_LOG(LogMetaSound, Verbose,
					TEXT("Preemptive load of class '%s' due to early "
						"registration request (asset scan likely not complete)."),
					*ClassKey);

				UObject* MetaSoundObject = Asset->GetOwningAsset();
				if (ensureAlways(MetaSoundObject))
				{
					AddOrUpdateAsset(*MetaSoundObject);
				}
			}
		}
		else
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Null referenced dependent asset in %s. Resaving asset in editor may fix the issue"), *InAssetBase.GetOwningAssetName());
		}
	}
}
#endif

Metasound::Frontend::FNodeRegistryKey UMetaSoundAssetSubsystem::AddOrUpdateAsset(const UObject& InObject)
{
	using namespace Metasound;
	using namespace Metasound::AssetSubsystemPrivate;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundAssetSubsystem::AddOrUpdateAsset);

	const FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InObject);
	check(MetaSoundAsset);

	FNodeClassInfo ClassInfo = MetaSoundAsset->GetAssetClassInfo();
	const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(ClassInfo);

	if (NodeRegistryKey::IsValid(RegistryKey))
	{
		PathMap.FindOrAdd(RegistryKey) = InObject.GetPathName();
	}

	return RegistryKey;
}

Metasound::Frontend::FNodeRegistryKey UMetaSoundAssetSubsystem::AddOrUpdateAsset(const FAssetData& InAssetData)
{
	using namespace Metasound;
	using namespace Metasound::AssetSubsystemPrivate;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundAssetSubsystem::AddOrUpdateAsset);

	FNodeClassInfo ClassInfo;
	bool bClassInfoFound = GetAssetClassInfo(InAssetData, ClassInfo);
	if (!bClassInfoFound)
	{
		UObject* Object = nullptr;

		FSoftObjectPath Path = InAssetData.ToSoftObjectPath();
		if (!FPackageName::GetPackageMountPoint(InAssetData.GetObjectPathString()).IsNone())
		{
			if (InAssetData.IsAssetLoaded())
			{
				Object = Path.ResolveObject();
				UE_LOG(LogMetaSound, Verbose, TEXT("Adding loaded asset '%s' to MetaSoundAsset registry."), *Object->GetName());
			}
			else
			{
				Object = Path.TryLoad();
				UE_LOG(LogMetaSound, Verbose, TEXT("Loaded asset '%s' and adding to MetaSoundAsset registry."), *Object->GetName());
			}
		}

		if (Object)
		{
			return AddOrUpdateAsset(*Object);
		}
	}

	if (ClassInfo.AssetClassID.IsValid())
	{
		const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(ClassInfo);
		if (NodeRegistryKey::IsValid(RegistryKey))
		{
			PathMap.FindOrAdd(RegistryKey) = InAssetData.GetSoftObjectPath();
		}

		return RegistryKey;
	}

	// Invalid ClassID means the node could not be registered.
	// Let caller report or ensure as necessary.
	return NodeRegistryKey::GetInvalid();
}

bool UMetaSoundAssetSubsystem::CanAutoUpdate(const FMetasoundFrontendClassName& InClassName) const
{
	const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
	if (!Settings->bAutoUpdateEnabled)
	{
		return false;
	}

	return !AutoUpdateDenyListCache.Contains(InClassName.GetFullName());
}

bool UMetaSoundAssetSubsystem::ContainsKey(const Metasound::Frontend::FNodeRegistryKey& InRegistryKey) const
{
	return PathMap.Contains(InRegistryKey);
}

void UMetaSoundAssetSubsystem::RebuildDenyListCache(const UAssetManager& InAssetManager)
{
	using namespace Metasound::Frontend;

	const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
	if (Settings->DenyListCacheChangeID == AutoUpdateDenyListChangeID)
	{
		return;
	}

	AutoUpdateDenyListCache.Reset();

	for (const FMetasoundFrontendClassName& ClassName : Settings->AutoUpdateDenylist)
	{
		AutoUpdateDenyListCache.Add(ClassName.GetFullName());
	}

	for (const FDefaultMetaSoundAssetAutoUpdateSettings& UpdateSettings : Settings->AutoUpdateAssetDenylist)
	{
		if (UAssetManager* AssetManager = UAssetManager::GetIfValid())
		{
			FAssetData AssetData;
			if (AssetManager->GetAssetDataForPath(UpdateSettings.MetaSound, AssetData))
			{
				FString AssetClassID;
				if (AssetData.GetTagValue(AssetTags::AssetClassID, AssetClassID))
				{
					const FMetasoundFrontendClassName ClassName = { FName(), *AssetClassID, FName() };
					AutoUpdateDenyListCache.Add(ClassName.GetFullName());
				}
			}
		}
	}

	AutoUpdateDenyListChangeID = Settings->DenyListCacheChangeID;
}

#if WITH_EDITOR
TSet<UMetaSoundAssetSubsystem::FAssetInfo> UMetaSoundAssetSubsystem::GetReferencedAssetClasses(const FMetasoundAssetBase& InAssetBase) const
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundAssetSubsystem::GetReferencedAssetClasses);
	using namespace Metasound::Frontend;

	if (!bIsInitialAssetScanComplete)
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Attempt to get registered dependent assets for %s before asset scan is complete may result in missed dependencies"), *InAssetBase.GetOwningAssetName());
	}

	TSet<FAssetInfo> OutAssetInfos;
	const FMetasoundFrontendDocument& Document = InAssetBase.GetDocumentChecked();
	for (const FMetasoundFrontendClass& Class : Document.Dependencies)
	{
		const FNodeRegistryKey Key = NodeRegistryKey::CreateKey(Class.Metadata);
		if (const FSoftObjectPath* ObjectPath = PathMap.Find(Key))
		{
			OutAssetInfos.Add(FAssetInfo{Key, *ObjectPath});
		}
	}
	return MoveTemp(OutAssetInfos);
}
#endif

void UMetaSoundAssetSubsystem::RescanAutoUpdateDenyList()
{
	if (const UAssetManager* AssetManager = UAssetManager::GetIfValid())
	{
		RebuildDenyListCache(*AssetManager);
	}
}

FMetasoundAssetBase* UMetaSoundAssetSubsystem::TryLoadAssetFromKey(const Metasound::Frontend::FNodeRegistryKey& RegistryKey) const
{
	if (const FSoftObjectPath* ObjectPath = FindObjectPathFromKey(RegistryKey))
	{
		return TryLoadAsset(*ObjectPath);
	}

	return nullptr;
}

bool UMetaSoundAssetSubsystem::TryLoadReferencedAssets(const FMetasoundAssetBase& InAssetBase, TArray<FMetasoundAssetBase*>& OutReferencedAssets) const
{
	using namespace Metasound::Frontend;

	bool bSucceeded = true;
	OutReferencedAssets.Reset();

	TArray<FMetasoundAssetBase*> ReferencedAssets;
	const TSet<FString>& AssetClassKeys = InAssetBase.GetReferencedAssetClassKeys();
	for (const FNodeRegistryKey& Key : AssetClassKeys)
	{
		if (FMetasoundAssetBase* MetaSound = TryLoadAssetFromKey(Key))
		{
			OutReferencedAssets.Add(MetaSound);
		}
		else
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to find referenced MetaSound asset with key '%s'"), *Key);
			bSucceeded = false;
		}
	}

	return bSucceeded;
}

void UMetaSoundAssetSubsystem::RequestAsyncLoadReferencedAssets(FMetasoundAssetBase& InAssetBase)
{
	const TSet<FSoftObjectPath>& AsyncReferences = InAssetBase.GetAsyncReferencedAssetClassPaths();
	if (AsyncReferences.Num() > 0)
	{
		if (UObject* OwningAsset = InAssetBase.GetOwningAsset())
		{
			TArray<FSoftObjectPath> PathsToLoad = AsyncReferences.Array();

			// Protect against duplicate calls to async load assets. 
			if (FMetaSoundAsyncAssetDependencies* ExistingAsyncLoad = FindLoadingDependencies(OwningAsset))
			{
				if (ExistingAsyncLoad->Dependencies == PathsToLoad)
				{
					// early out since these are already actively being loaded.
					return;
				}
			}

			int32 AsyncLoadID = AsyncLoadIDCounter++;

			auto AssetsLoadedDelegate = [this, AsyncLoadID]()
			{
				this->OnAssetsLoaded(AsyncLoadID);
			};

			// Store async loading data for use when async load is complete. 
			FMetaSoundAsyncAssetDependencies& AsyncDependencies = LoadingDependencies.AddDefaulted_GetRef();

			AsyncDependencies.LoadID = AsyncLoadID;
			AsyncDependencies.MetaSound = OwningAsset;
			AsyncDependencies.Dependencies = PathsToLoad;
			AsyncDependencies.StreamableHandle = StreamableManager.RequestAsyncLoad(PathsToLoad, AssetsLoadedDelegate);
		}
		else
		{
			UE_LOG(LogMetaSound, Error, TEXT("Cannot load async asset as FMetasoundAssetBase null owning UObject"), *InAssetBase.GetOwningAssetName());
		}
	}

}

void UMetaSoundAssetSubsystem::WaitUntilAsyncLoadReferencedAssetsComplete(FMetasoundAssetBase& InAssetBase)
{
	UObject* OwningAsset = InAssetBase.GetOwningAsset();
	if (OwningAsset)
	{
		while (FMetaSoundAsyncAssetDependencies* LoadingDependency = FindLoadingDependencies(OwningAsset))
		{
			// Grab shared ptr to handle as LoadingDependencies may be deleted and have it's shared pointer removed. 
			TSharedPtr<FStreamableHandle> StreamableHandle = LoadingDependency->StreamableHandle;
			if (StreamableHandle.IsValid())
			{
				UE_LOG(LogMetaSound, Verbose, TEXT("Waiting on async load (id: %d) from asset %s"), LoadingDependency->LoadID, *InAssetBase.GetOwningAssetName());

				EAsyncPackageState::Type LoadState = StreamableHandle->WaitUntilComplete();
				if (EAsyncPackageState::Complete != LoadState)
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to complete loading of async dependent assets from parent asset %s"), *InAssetBase.GetOwningAssetName());
					RemoveLoadingDependencies(LoadingDependency->LoadID);
				}
				else
				{
					// This will remove the loading dependencies from internal storage
					OnAssetsLoaded(LoadingDependency->LoadID);
				}

				// This will prevent OnAssetsLoaded from being called via the streamables
				// internal delegate complete callback.
				StreamableHandle->CancelHandle();
			}
		}
	}
}

FMetaSoundAsyncAssetDependencies* UMetaSoundAssetSubsystem::FindLoadingDependencies(const UObject* InParentAsset)
{
	auto IsEqualMetaSoundUObject = [InParentAsset](const FMetaSoundAsyncAssetDependencies& InDependencies) -> bool
	{
		return (InDependencies.MetaSound == InParentAsset);
	}; 

	return LoadingDependencies.FindByPredicate(IsEqualMetaSoundUObject);
}

FMetaSoundAsyncAssetDependencies* UMetaSoundAssetSubsystem::FindLoadingDependencies(int32 InLoadID)
{
	auto IsEqualID = [InLoadID](const FMetaSoundAsyncAssetDependencies& InDependencies) -> bool
	{
		return (InDependencies.LoadID == InLoadID);
	};
	
	return LoadingDependencies.FindByPredicate(IsEqualID);
}

void UMetaSoundAssetSubsystem::RemoveLoadingDependencies(int32 InLoadID)
{
	auto IsEqualID = [InLoadID](const FMetaSoundAsyncAssetDependencies& InDependencies) -> bool
	{
		return (InDependencies.LoadID == InLoadID);
	};
	LoadingDependencies.RemoveAllSwap(IsEqualID);
}

void UMetaSoundAssetSubsystem::OnAssetsLoaded(int32 InLoadID)
{
	FMetaSoundAsyncAssetDependencies* LoadedDependencies = FindLoadingDependencies(InLoadID);
	if (ensureMsgf(LoadedDependencies, TEXT("Call to async asset load complete with invalid IDs %d"), InLoadID))
	{
		if (LoadedDependencies->StreamableHandle.IsValid())
		{
			if (LoadedDependencies->MetaSound)
			{
				Metasound::IMetasoundUObjectRegistry& UObjectRegistry = Metasound::IMetasoundUObjectRegistry::Get();
				FMetasoundAssetBase* ParentAssetBase = UObjectRegistry.GetObjectAsAssetBase(LoadedDependencies->MetaSound);
				if (ensureMsgf(ParentAssetBase, TEXT("UClass of Parent MetaSound asset %s is not registered in metasound UObject Registery"), *LoadedDependencies->MetaSound->GetPathName()))
				{
					// Get all async loaded assets
					TArray<UObject*> LoadedAssets;
					LoadedDependencies->StreamableHandle->GetLoadedAssets(LoadedAssets);

					// Cast UObjects to FMetaSoundAssetBase
					TArray<FMetasoundAssetBase*> LoadedAssetBases;
					for (UObject* AssetDependency : LoadedAssets)
					{
						if (AssetDependency)
						{
							FMetasoundAssetBase* AssetDependencyBase = UObjectRegistry.GetObjectAsAssetBase(AssetDependency);
							if (ensure(AssetDependencyBase))
							{
								LoadedAssetBases.Add(AssetDependencyBase);
							}
						}
					}

					// Update parent asset with loaded assets. 
					ParentAssetBase->OnAsyncReferencedAssetsLoaded(LoadedAssetBases);
				}
			}
		}

		// Remove from active array of loading dependencies.
		RemoveLoadingDependencies(InLoadID);
	}
}

const FSoftObjectPath* UMetaSoundAssetSubsystem::FindObjectPathFromKey(const Metasound::Frontend::FNodeRegistryKey& InRegistryKey) const
{
	return PathMap.Find(InRegistryKey);
}

FMetasoundAssetBase* UMetaSoundAssetSubsystem::TryLoadAsset(const FSoftObjectPath& InObjectPath) const
{
	return Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(InObjectPath.TryLoad());
}

void UMetaSoundAssetSubsystem::RemoveAsset(const UObject& InObject)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	if (const FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InObject))
	{
		const FNodeClassInfo ClassInfo = MetaSoundAsset->GetAssetClassInfo();
		FNodeRegistryKey RegistryKey = FMetasoundFrontendRegistryContainer::Get()->GetRegistryKey(ClassInfo);
		AssetSubsystemPrivate::RemoveIfExactMatch(PathMap, RegistryKey, FSoftObjectPath(&InObject));
	}
}

void UMetaSoundAssetSubsystem::RemoveAsset(const FAssetData& InAssetData)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FNodeClassInfo ClassInfo;
	if (ensureAlways(AssetSubsystemPrivate::GetAssetClassInfo(InAssetData, ClassInfo)))
	{
		FNodeRegistryKey RegistryKey = FMetasoundFrontendRegistryContainer::Get()->GetRegistryKey(ClassInfo);
		AssetSubsystemPrivate::RemoveIfExactMatch(PathMap, RegistryKey, InAssetData.GetSoftObjectPath());
	}
}

void UMetaSoundAssetSubsystem::RenameAsset(const FAssetData& InAssetData, bool bInReregisterWithFrontend)
{
	auto PerformRename = [this, &InAssetData]()
	{
		RemoveAsset(InAssetData);
		ResetAssetClassDisplayName(InAssetData);
		AddOrUpdateAsset(InAssetData);
	};

	if (bInReregisterWithFrontend)
	{
		FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(InAssetData.GetAsset());
		check(MetaSoundAsset);

		MetaSoundAsset->UnregisterGraphWithFrontend();
		PerformRename();
		MetaSoundAsset->RegisterGraphWithFrontend();
	}
	else
	{
		PerformRename();
	}
}

void UMetaSoundAssetSubsystem::ResetAssetClassDisplayName(const FAssetData& InAssetData)
{
	UObject* Object = nullptr;
	FSoftObjectPath Path = InAssetData.GetSoftObjectPath();
	if (InAssetData.IsAssetLoaded())
	{
		Object = Path.ResolveObject();
	}
	else
	{
		Object = Path.TryLoad();
	}

	FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Object);
	check(MetaSoundAsset);
	FMetasoundFrontendGraphClass& Class = MetaSoundAsset->GetDocumentChecked().RootGraph;

#if WITH_EDITOR
	Class.Metadata.SetDisplayName(FText());
#endif // WITH_EDITOR
}

void UMetaSoundAssetSubsystem::SearchAndIterateDirectoryAssets(const TArray<FDirectoryPath>& InDirectories, TFunctionRef<void(const FAssetData&)> InFunction)
{
	if (InDirectories.IsEmpty())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();

	FAssetManagerSearchRules Rules;
	for (const FDirectoryPath& Path : InDirectories)
	{
		Rules.AssetScanPaths.Add(*Path.Path);
	}

	Metasound::IMetasoundUObjectRegistry::Get().IterateRegisteredUClasses([&](UClass& RegisteredClass)
	{
		Rules.AssetBaseClass = &RegisteredClass;
		TArray<FAssetData> MetaSoundAssets;
		AssetManager.SearchAssetRegistryPaths(MetaSoundAssets, Rules);
		for (const FAssetData& AssetData : MetaSoundAssets)
		{
			InFunction(AssetData);
		}
	});
}

void UMetaSoundAssetSubsystem::RegisterAssetClassesInDirectories(const TArray<FMetaSoundAssetDirectory>& InDirectories)
{
	TArray<FDirectoryPath> Directories;
	Algo::Transform(InDirectories, Directories, [](const FMetaSoundAssetDirectory& AssetDir) { return AssetDir.Directory; });

	SearchAndIterateDirectoryAssets(Directories, [this](const FAssetData& AssetData)
	{
		AddOrUpdateAsset(AssetData);
		FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(AssetData.GetAsset());
		check(MetaSoundAsset);

		Metasound::Frontend::FMetaSoundAssetRegistrationOptions RegOptions;
		if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
		{
			RegOptions.bAutoUpdateLogWarningOnDroppedConnection = Settings->bAutoUpdateLogWarningOnDroppedConnection;
		}
		MetaSoundAsset->RegisterGraphWithFrontend(RegOptions);
	});
}

void UMetaSoundAssetSubsystem::UnregisterAssetClassesInDirectories(const TArray<FMetaSoundAssetDirectory>& InDirectories)
{
	TArray<FDirectoryPath> Directories;
	Algo::Transform(InDirectories, Directories, [](const FMetaSoundAssetDirectory& AssetDir) { return AssetDir.Directory; });

	SearchAndIterateDirectoryAssets(Directories, [this](const FAssetData& AssetData)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;

		if (AssetData.IsAssetLoaded())
		{
			FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(AssetData.GetAsset());
			check(MetaSoundAsset);
			MetaSoundAsset->UnregisterGraphWithFrontend();

			RemoveAsset(AssetData);
		}
		else
		{
			FNodeClassInfo AssetClassInfo;
			if (ensureAlways(AssetSubsystemPrivate::GetAssetClassInfo(AssetData, AssetClassInfo)))
			{
				const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(AssetClassInfo);
				const bool bIsRegistered = FMetasoundFrontendRegistryContainer::Get()->IsNodeRegistered(RegistryKey);
				if (bIsRegistered)
				{
					FMetasoundFrontendRegistryContainer::Get()->UnregisterNode(RegistryKey);
					AssetSubsystemPrivate::RemoveIfExactMatch(PathMap, RegistryKey, AssetData.GetSoftObjectPath());
				}
			}
		}
	});
}

