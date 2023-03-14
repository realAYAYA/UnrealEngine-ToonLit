// Copyright Epic Games, Inc. All Rights Reserved.

#include "CollectionAssetRegistryBridge.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CollectionManagerModule.h"
#include "CollectionManagerTypes.h"
#include "Containers/Set.h"
#include "ContentBrowserLog.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "ICollectionManager.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Trace/Detail/Channel.h"
#include "UObject/Class.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/LinkerLoad.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

/** The collection manager doesn't know how to follow redirectors, this class provides it with that knowledge */
class FCollectionRedirectorFollower : public ICollectionRedirectorFollower
{
public:
	FCollectionRedirectorFollower()
		: AssetRegistryModule(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")))
	{
	}

	virtual bool FixupObject(const FSoftObjectPath& InObjectPath, FSoftObjectPath& OutNewObjectPath) override
	{
		OutNewObjectPath = FSoftObjectPath();

		FString InObjectPathString = InObjectPath.ToString();
		if (!InObjectPathString.StartsWith(TEXT("/")))
		{
			if (!GIsSavingPackage)
			{
				FTopLevelAssetPath FullPath = UClass::TryConvertShortTypeNameToPathName(UClass::StaticClass(), InObjectPathString);
				if (FullPath.IsValid())
				{
					FString ClassPathStr = FullPath.ToString();
					const FString NewClassName = FLinkerLoad::FindNewPathNameForClass(ClassPathStr, false);
					if (!NewClassName.IsEmpty())
					{
						check(FPackageName::IsValidObjectPath(NewClassName));
						// Our new class name might be lacking the path, so try and find it so we can use the full path in the collection
						UClass* FoundClass = FindObject<UClass>(nullptr, *NewClassName);
						if (FoundClass)
						{
							OutNewObjectPath = *FoundClass->GetPathName();
						}
					}
				}
			}
		}
		else if (InObjectPathString.StartsWith(TEXT("/Script/"))) 
		{
			// We can't use FindObject while we're saving
			if (!GIsSavingPackage)
			{
				const FString ClassPathStr = InObjectPath.ToString();
				check(FPackageName::IsValidObjectPath(ClassPathStr));
				UClass* FoundClass = FindObject<UClass>(nullptr, *ClassPathStr);
				if (!FoundClass)
				{
					// Use the linker to search for class name redirects (from the loaded ActiveClassRedirects)
					const FString NewClassName = FLinkerLoad::FindNewPathNameForClass(ClassPathStr, false);

					if (!NewClassName.IsEmpty())
					{
						check(FPackageName::IsValidObjectPath(NewClassName));
						// Our new class name might be lacking the path, so try and find it so we can use the full path in the collection
						FoundClass = FindObject<UClass>(nullptr, *NewClassName);
						if (FoundClass)
						{
							OutNewObjectPath = *FoundClass->GetPathName();
						}
					}
				}
			}
		}
		else
		{
			// Keep track of visted redirectors in case we loop.
			TSet<FSoftObjectPath> VisitedRedirectors;

			// Use the asset registry to avoid loading the object
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FAssetData ObjectAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(InObjectPath, true);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			while (ObjectAssetData.IsValid() && ObjectAssetData.IsRedirector())
			{
				// Check to see if we've already seen this path before, it's possible we might have found a redirector loop.
				if ( VisitedRedirectors.Contains(ObjectAssetData.ToSoftObjectPath()) )
				{
					UE_LOG(LogContentBrowser, Error, TEXT("Redirector Loop Found!"));
					for ( const FSoftObjectPath& Redirector : VisitedRedirectors )
					{
						UE_LOG(LogContentBrowser, Error, TEXT("Redirector: %s"), *Redirector.ToString());
					}

					ObjectAssetData = FAssetData();
					break;
				}

				VisitedRedirectors.Add(ObjectAssetData.ToSoftObjectPath());

				// Get the destination object from the meta-data rather than load the redirector object, as 
				// loading a redirector will also load the object it points to, which could cause a large hitch
				FString DestinationObjectPath;
				if (ObjectAssetData.GetTagValue("DestinationObject", DestinationObjectPath))
				{
					ConstructorHelpers::StripObjectClass(DestinationObjectPath);
					ObjectAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(DestinationObjectPath));
				}
				else
				{
					ObjectAssetData = FAssetData();
				}
			}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
			OutNewObjectPath = ObjectAssetData.ObjectPath;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		return OutNewObjectPath.IsValid() && InObjectPath != OutNewObjectPath;
	}

private:
	FAssetRegistryModule& AssetRegistryModule;
};

FCollectionAssetRegistryBridge::FCollectionAssetRegistryBridge()
{
	// Load the asset registry module to listen for updates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FCollectionAssetRegistryBridge::OnAssetRemoved);
	AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &FCollectionAssetRegistryBridge::OnAssetRenamed);
	
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FCollectionAssetRegistryBridge::OnAssetRegistryLoadComplete);
	}
	else
	{
		OnAssetRegistryLoadComplete();
	}
}

FCollectionAssetRegistryBridge::~FCollectionAssetRegistryBridge()
{
	// Load the asset registry module to unregister delegates
	if (FModuleManager::Get().IsModuleLoaded("AssetRegistry"))
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll(this);
		AssetRegistryModule.Get().OnAssetRenamed().RemoveAll(this);
		AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);
	}
}

void FCollectionAssetRegistryBridge::OnAssetRegistryLoadComplete()
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	// We've found all the assets, let the collections manager fix up its references now so that it doesn't reference any redirectors
	FCollectionRedirectorFollower RedirectorFollower;
	CollectionManagerModule.Get().HandleFixupRedirectors(RedirectorFollower);
}

void FCollectionAssetRegistryBridge::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	// Notify the collections manager that an asset has been renamed
	CollectionManagerModule.Get().HandleObjectRenamed(FSoftObjectPath(OldObjectPath), AssetData.GetSoftObjectPath());
}

void FCollectionAssetRegistryBridge::OnAssetRemoved(const FAssetData& AssetData)
{
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	if (AssetData.IsRedirector())
	{
		// Notify the collections manager that a redirector has been removed
		// This will attempt to re-save any collections that still have a reference to this redirector in their on-disk collection data
		CollectionManagerModule.Get().HandleRedirectorDeleted(AssetData.GetSoftObjectPath());
	}
	else
	{
		// Notify the collections manager that an asset has been removed
		CollectionManagerModule.Get().HandleObjectDeleted(AssetData.GetSoftObjectPath());
	}
}

#undef LOCTEXT_NAMESPACE
