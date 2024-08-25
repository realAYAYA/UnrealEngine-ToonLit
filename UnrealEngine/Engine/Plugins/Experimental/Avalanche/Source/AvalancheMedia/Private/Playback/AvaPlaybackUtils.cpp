// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvaPlaybackUtils.h"

#include "AssetRegistry/AssetData.h"
#include "AvaAssetTags.h"
#include "AvaMediaModule.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "UObject/Linker.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Editor.h"
#include "PackageTools.h"
#include "Selection.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif

void FAvaPlaybackUtils::FlushPackageLoading(UPackage* InPackage)
{
	if (!InPackage->IsFullyLoaded())
	{
		FlushAsyncLoading();
		InPackage->FullyLoad();
	}
	ResetLoaders(InPackage);
}

bool FAvaPlaybackUtils::IsPackageDeleted(const UPackage* InExistingPackage)
{
	if (!InExistingPackage)
	{
		return false;
	}
	
	const FString PackageExtension = InExistingPackage->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(InExistingPackage->GetName(), PackageExtension);
			
	return !FPaths::FileExists(PackageFilename);
}

namespace UE::AvaPlaybackUtils::Private
{
#if WITH_EDITOR
	void CollectObjectToPurge(UObject* InObject, TArray<UObject*>& OutObjectsToPurge)
	{
		if (InObject->IsAsset() && GIsEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(InObject);
			GEditor->GetSelectedObjects()->Deselect(InObject);
		}
		OutObjectsToPurge.Add(InObject);
	}
#endif
}

// Adapted from: PurgePackages in ConcertSyncClientUtil.cpp
// Notes:
// - The method used in USourceControlHelpers::ApplyOperationAndReloadPackages with the
//   asset registry and ObjectTools::DeleteObjectsUnchecked can't be used because we want
//   this to work in game mode.
// - We assume the assets purged will not be the edited world (current level editor world)
//   so we can skip the special case from the original code.
void FAvaPlaybackUtils::PurgePackages(const TArray<UPackage*>& InExistingPackages)
{
#if WITH_EDITOR
	using namespace UE::AvaPlaybackUtils::Private;

	TArray<UObject*> ObjectsToPurge;

	for (UPackage* ExistingPackage : InExistingPackages)
	{
		if (!IsValid(ExistingPackage))
		{
			continue;
		}
		
		// Prevent any message from the editor saying a package is not saved or doesn't exist on disk.
		ExistingPackage->SetDirtyFlag(false);

		CollectObjectToPurge(ExistingPackage, ObjectsToPurge);
		ForEachObjectWithPackage(ExistingPackage, [&ObjectsToPurge](UObject* InObject)
		{
			CollectObjectToPurge(InObject, ObjectsToPurge);
			return true;
		});
	}
	
	// Broadcast the eminent objects destruction (ex. tell BlueprintActionDatabase to release its reference(s) on Blueprint(s) right now)
	FEditorDelegates::OnAssetsPreDelete.Broadcast(ObjectsToPurge);

	// Mark objects as purgeable.
	for (UObject* Object : ObjectsToPurge)
	{
		if (Object->IsRooted())
		{
			Object->RemoveFromRoot();
		}
		Object->ClearFlags(RF_Public | RF_Standalone);
	}

	if (ObjectsToPurge.Num() > 0)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
#endif
}

// Adapted from HotReloadPackages in ConcertSyncClientUtil.cpp
bool FAvaPlaybackUtils::ReloadPackages(const TArray<UPackage*>& InExistingPackages)
{
	FlushAsyncLoading();
	{
		bool bRunGC = false;
		for (UPackage* Package : InExistingPackages)
		{
			if (FLinkerLoad::RemoveKnownMissingPackage(Package->GetFName()))
			{
				UE_LOG(LogAvaMedia, Verbose, TEXT("Package \"%s\" was removed from known missing."), *Package->GetName());
				bRunGC = true;
			}
			if (Package->HasAnyPackageFlags(PKG_NewlyCreated))
			{
				UE_LOG(LogAvaMedia, Verbose, TEXT("Clearing Newly Created flag on Package \"%s\"."), *Package->GetName());
				Package->ClearPackageFlags(PKG_NewlyCreated);
			}
		}
		if (bRunGC)
		{
			UE_LOG(LogAvaMedia, Log, TEXT("Some packages where removed from known missing, garbage collecting ..."));
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}
	FlushRenderingCommands();

	FText ErrorMessage;
#if WITH_EDITOR
	UPackageTools::ReloadPackages(InExistingPackages, ErrorMessage, UPackageTools::EReloadPackagesInteractionMode::AssumePositive);
#endif
	
	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogAvaMedia, Error, TEXT("%s"), *ErrorMessage.ToString());
		return false;
	}
	return true;
}

bool FAvaPlaybackUtils::IsMapAsset(const FString& InPackageName)
{
	FString FullPath;
	if (FPackageName::TryConvertLongPackageNameToFilename(InPackageName, FullPath, FPackageName::GetMapPackageExtension()))
	{
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath))
		{
			return true;
		}
	}
	return false;
}

bool FAvaPlaybackUtils::IsPlayableAsset(const FAssetData& InAssetData)
{
	const EMotionDesignAssetType AssetType = FAvaSoftAssetPath::GetAssetTypeFromClass(InAssetData.GetClass(), true);
	if (AssetType == EMotionDesignAssetType::Unknown)
	{
		return false;
	}
	// For world type, we need to check the tags.
	if (AssetType == EMotionDesignAssetType::World)
	{
		const FAssetTagValueRef SceneTag = InAssetData.TagsAndValues.FindTag(UE::Ava::AssetTags::MotionDesignScene);
		if (!SceneTag.IsSet() || !SceneTag.Equals(UE::Ava::AssetTags::Values::Enabled))
		{
			return false;
		}
	}
	return true;
}


namespace UE::AvaPlayback::Utils
{
	FString GetBriefFrameInfo()
	{
		return FString::Printf(TEXT("[%d]"), GFrameNumber);
	}
}