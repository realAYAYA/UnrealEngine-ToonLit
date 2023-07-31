// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorAssetLibrary.h"

#include "EditorScriptingUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Algo/Count.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "ISourceControlModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/MetaData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorAssetLibrary)

#define LOCTEXT_NAMESPACE "Editoribrary"

namespace InternalEditorLevelLibrary
{
	bool ListAssetDatas(const FString& AnyAssetPathOrAnyDirectoryPath, bool bRecursive, bool& bOutIsFolder, TArray<FAssetData>& OutResult, FString& OutValidDirectoryPath, FString& OutFailureReason);
	bool ListAssetDatasForDirectory(const FString& AnyDirectoryPath, bool bRecursive, TArray<FAssetData>& OutResult, FString& OutValidDirectoryPath, FString& OutFailureReason);

	bool IsAssetRegistryModuleLoading()
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("The AssetRegistry is currently loading."));
			return false;
		}
		return true;
	}

	UObject* LoadAsset(const FString& AssetPath, bool bAllowMapAsset, FString& OutFailureReason)
	{
		FAssetData AssetData = EditorScriptingUtils::FindAssetDataFromAnyPath(AssetPath, OutFailureReason);
		if (!AssetData.IsValid())
		{
			return nullptr;
		}
		return EditorScriptingUtils::LoadAsset(AssetData, bAllowMapAsset, OutFailureReason);
	}

	bool GetContentBrowserPackagesForDirectory(const FString& AnyDirectoryPath, bool bOnlyIfIsDirty, bool bRecursive, TArray<UPackage*>& OutResult, FString& OutFailureReason)
	{
		FString ValidDirectoryPath;
		TArray<FAssetData> AssetDatas;
		if (!ListAssetDatasForDirectory(AnyDirectoryPath, bRecursive, AssetDatas, ValidDirectoryPath, OutFailureReason))
		{
			return false;
		}

		if (bOnlyIfIsDirty)
		{
			for (const FAssetData& AssetData : AssetDatas)
			{
				// Can't be dirty is not loaded
				if (AssetData.IsAssetLoaded())
				{
					UPackage* Package = AssetData.GetPackage();
					if (Package && Package->IsDirty())
					{
						Package->FullyLoad();
						OutResult.AddUnique(Package);
					}
				}
			}
		}
		else
		{
			// load all assets
			for (const FAssetData& AssetData : AssetDatas)
			{
				UPackage* Package = AssetData.GetPackage();
				if (Package)
				{
					Package->FullyLoad();
					OutResult.AddUnique(Package);
				}
			}
		}

		return true;
	}

	// Valid inputs: "/Game/MyFolder/", "/Game/MyFolder", "/Game/", "/Game"
	bool ListAssetDatasForDirectory(const FString& AnyPathDirectoryPath, bool bRecursive, TArray<FAssetData>& OutResult, FString& OutValidDirectoryPath, FString& OutFailureReason)
	{
		OutResult.Reset();
		OutValidDirectoryPath.Reset();

		OutValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(AnyPathDirectoryPath, OutFailureReason);
		if (OutValidDirectoryPath.IsEmpty())
		{
			return false;
		}

		TArray<FAssetData> MapAssetDatas;
		return EditorScriptingUtils::GetAssetsInPath(OutValidDirectoryPath, bRecursive, OutResult, MapAssetDatas, OutFailureReason);
	}

	// Valid inputs: "/Game/MyFolder/MyAsset.MyAsset", "/Game/MyFolder/MyAsset", "/Game/MyFolder/", "/Game/MyFolder", "/Game/", "/Game"
	bool ListAssetDatas(const FString& AnyAssetPathOrAnyDirectoryPath, bool bRecursive, bool& bOutIsFolder, TArray<FAssetData>& OutResult, FString& OutValidDirectoryPath, FString& OutFailureReason)
	{
		OutResult.Reset();
		OutValidDirectoryPath.Reset();
		bOutIsFolder = false;

		// Ask the AssetRegistry if it's a file
		FAssetData AssetData = EditorScriptingUtils::FindAssetDataFromAnyPath(AnyAssetPathOrAnyDirectoryPath, OutFailureReason);
		if (AssetData.IsValid())
		{
			if (EditorScriptingUtils::IsPackageFlagsSupportedForAssetLibrary(AssetData.PackageFlags))
			{
				OutResult.Add(AssetData);
			}
		}
		else
		{
			bOutIsFolder = true;
			return ListAssetDatasForDirectory(AnyAssetPathOrAnyDirectoryPath, bRecursive, OutResult, OutValidDirectoryPath, OutFailureReason);
		}

		return true;
	}

	struct FValidatedPaths
	{
		FString SourceValidDirectoryPath;
		FString SourceFilePath;
		FString DestinationValidDirectoryPath;
		FString DestinationFilePath;
	};
	struct FValidatedObjectInfos
	{
		TArray<UObject*> PreviousLoadedAssets;
		TArray<FString> NewAssetsDirectoryPath;
		void Reset()
		{
			PreviousLoadedAssets.Reset();
			NewAssetsDirectoryPath.Reset();
		}
	};
	bool ValidateSourceAndDestinationForOperation(const FString& SourceDirectoryPath, const FString& DestinationDirectoryPath, bool bValidIfOnlyAllAssetCanBeOperatedOn, const TCHAR* CommandName, FValidatedPaths& OutValidatedPaths, FValidatedObjectInfos& OutObjectInfos)
	{
		// Test the path to see if they are valid
		FString FailureReason;
		OutValidatedPaths.SourceValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(SourceDirectoryPath, FailureReason);
		if (OutValidatedPaths.SourceValidDirectoryPath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to convert the source path. %s"), CommandName, *FailureReason);
			return false;
		}

		OutValidatedPaths.SourceFilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(OutValidatedPaths.SourceValidDirectoryPath));
		if (OutValidatedPaths.SourceFilePath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to convert the source path '%s' to a full path. Was it rooted?"), CommandName, *OutValidatedPaths.SourceValidDirectoryPath);
			return false;
		}

		OutValidatedPaths.DestinationValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(DestinationDirectoryPath, FailureReason);
		if (OutValidatedPaths.DestinationValidDirectoryPath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to convert the destination path. %s"), CommandName, *FailureReason);
			return false;
		}

		OutValidatedPaths.DestinationFilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(OutValidatedPaths.DestinationValidDirectoryPath));
		if (OutValidatedPaths.DestinationFilePath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to convert the destination path '%s' to a full path. Was it rooted?"), CommandName, *OutValidatedPaths.DestinationFilePath);
			return false;
		}

		// If the directory doesn't exist on drive then can't rename/duplicate it
		if (!IFileManager::Get().DirectoryExists(*OutValidatedPaths.SourceFilePath))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. The source directory do not exist."), CommandName);
			return false;
		}

		// Create the distination directory if it doesn't already exist
		if (!IFileManager::Get().DirectoryExists(*OutValidatedPaths.DestinationFilePath))
		{
			const bool bTree = true;
			if (!IFileManager::Get().MakeDirectory(*OutValidatedPaths.DestinationFilePath, bTree))
			{
				UE_LOG(LogEditorScripting, Error, TEXT("%s. The destination directory could not be created."), CommandName);
				return false;
			}
		}

		// Select all the asset the folder contains
		// Because we want to rename a folder, we can't rename any files that can't be deleted
		TArray<FAssetData> CouldNotLoadAssetData;
		TArray<FString> FailureReasons;
		bool bFailedToGetLoadedAssets = !EditorScriptingUtils::GetAssetsInPath(OutValidatedPaths.SourceValidDirectoryPath, true, OutObjectInfos.PreviousLoadedAssets, CouldNotLoadAssetData, FailureReasons);
		if (bFailedToGetLoadedAssets && bValidIfOnlyAllAssetCanBeOperatedOn)
		{
			bFailedToGetLoadedAssets = CouldNotLoadAssetData.Num() > 0;
		}
		if (bFailedToGetLoadedAssets)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to validate all assets."), CommandName);
			for (const FString& Reason : FailureReasons)
			{
				UE_LOG(LogEditorScripting, Error, TEXT("  %s"), *Reason);
			}
			return false;
		}

		// Test to see if the destination will be valid
		if (OutObjectInfos.PreviousLoadedAssets.Num())
		{
			for (UObject* Object : OutObjectInfos.PreviousLoadedAssets)
			{
				FString ObjectPackageName = Object->GetOutermost()->GetName();
				FString ObjectLongPackagePath = FPackageName::GetLongPackagePath(ObjectPackageName);
				FString ObjectName = FPackageName::ObjectPathToObjectName(ObjectPackageName);

				// Remove source from the object name
				ObjectLongPackagePath.MidInline(OutValidatedPaths.SourceValidDirectoryPath.Len(), MAX_int32, false);

				// Create AssetPath /Game/MyFolder/MyAsset.MyAsset
				FString NewAssetPackageName;
				if (ObjectLongPackagePath.IsEmpty())
				{
					NewAssetPackageName = FString::Printf(TEXT("%s/%s.%s"), *OutValidatedPaths.DestinationValidDirectoryPath, *Object->GetName(), *Object->GetName());
				}
				else
				{
					NewAssetPackageName = FString::Printf(TEXT("%s%s/%s.%s"), *OutValidatedPaths.DestinationValidDirectoryPath, *ObjectLongPackagePath, *Object->GetName(), *Object->GetName());
				}

				if (!EditorScriptingUtils::IsAValidPathForCreateNewAsset(NewAssetPackageName, FailureReason))
				{
					UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to validate the destination for asset '%s'. %s"), CommandName, *Object->GetName(), *FailureReason);
					OutObjectInfos.Reset();
					return false;
				}

				// Rename should do it, but will suggest another location via a Modal
				if (FPackageName::DoesPackageExist(NewAssetPackageName))
				{
					UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to validate the destination for asset '%s'. There's alreay an asset at the destination."), CommandName, *NewAssetPackageName);
					OutObjectInfos.Reset();
					return false;
				}

				// Keep AssetPath /Game/MyFolder
				OutObjectInfos.NewAssetsDirectoryPath.Add(FPackageName::GetLongPackagePath(NewAssetPackageName));
			}
		}

		return true;
	}
}

/**
 *
 * Load operations
 *
 **/

// A wrapper around
//unreal.AssetRegistryHelpers.get_asset(unreal.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path("/Game/NewDataTable.NewDataTable"))
UObject* UEditorAssetLibrary::LoadAsset(const FString& AssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->LoadAsset(AssetPath);
}

UClass* UEditorAssetLibrary::LoadBlueprintClass(const FString& AssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->LoadBlueprintClass(AssetPath);
}

FString UEditorAssetLibrary::GetPathNameForLoadedAsset(UObject* LoadedAsset)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->GetPathNameForLoadedAsset(LoadedAsset);
}

FAssetData UEditorAssetLibrary::FindAssetData(const FString& AssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->FindAssetData(AssetPath);
}

bool UEditorAssetLibrary::DoesAssetExist(const FString& AssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DoesAssetExist(AssetPath);
}

bool UEditorAssetLibrary::DoAssetsExist(const TArray<FString>& AssetPaths)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DoAssetsExist(AssetPaths);
}

TArray<FString> UEditorAssetLibrary::FindPackageReferencersForAsset(const FString& AnyAssetPath, bool bLoadAssetsToConfirm)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->FindPackageReferencersForAsset(AnyAssetPath, bLoadAssetsToConfirm);
}

bool UEditorAssetLibrary::ConsolidateAssets(UObject* AssetToConsolidateTo, const TArray<UObject*>& AssetsToConsolidate)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->ConsolidateAssets(AssetToConsolidateTo, AssetsToConsolidate);
}

/**
 *
 * Delete operations
 *
 **/

bool UEditorAssetLibrary::DeleteLoadedAsset(UObject* AssetToDelete)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DeleteLoadedAsset(AssetToDelete);
}

bool UEditorAssetLibrary::DeleteLoadedAssets(const TArray<UObject*>& AssetsToDelete)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DeleteLoadedAssets(AssetsToDelete);
}

bool UEditorAssetLibrary::DeleteAsset(const FString& AssetPathToDelete)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DeleteAsset(AssetPathToDelete);
}

bool UEditorAssetLibrary::DeleteDirectory(const FString& DirectoryPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DeleteDirectory(DirectoryPath);
}

/**
 *
 * Duplicate operations
 *
 **/

UObject* UEditorAssetLibrary::DuplicateLoadedAsset(UObject* SourceAsset, const FString& DestinationAssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DuplicateLoadedAsset(SourceAsset, DestinationAssetPath);
}

UObject* UEditorAssetLibrary::DuplicateAsset(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DuplicateAsset(SourceAssetPath, DestinationAssetPath);
}

bool UEditorAssetLibrary::DuplicateDirectory(const FString& SourceDirectoryPath, const FString& DestinationDirectoryPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DuplicateDirectory(SourceDirectoryPath, DestinationDirectoryPath);
}

/**
 *
 * Rename operations
 *
 **/

bool UEditorAssetLibrary::RenameLoadedAsset(UObject* SourceAsset, const FString& DestinationAssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->RenameLoadedAsset(SourceAsset, DestinationAssetPath);
}

bool UEditorAssetLibrary::RenameAsset(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->RenameAsset(SourceAssetPath, DestinationAssetPath);
}

bool UEditorAssetLibrary::RenameDirectory(const FString& SourceDirectoryPath, const FString& DestinationDirectoryPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->RenameDirectory(SourceDirectoryPath, DestinationDirectoryPath);
}

/**
 *
 * Checkout operations
 *
 **/

bool UEditorAssetLibrary::CheckoutLoadedAsset(UObject* AssetToCheckout)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->CheckoutLoadedAsset(AssetToCheckout);
}

bool UEditorAssetLibrary::CheckoutLoadedAssets(const TArray<UObject*>& AssetsToCheckout)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->CheckoutLoadedAssets(AssetsToCheckout);
}

bool UEditorAssetLibrary::CheckoutAsset(const FString& AssetsToCheckout)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->CheckoutAsset(AssetsToCheckout);
}

bool UEditorAssetLibrary::CheckoutDirectory(const FString& DirectoryPath, bool bRecursive)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->CheckoutDirectory(DirectoryPath, bRecursive);
}

/**
 *
 * Save operation
 *
 **/

bool UEditorAssetLibrary::SaveLoadedAsset(UObject* AssetToSave, bool bOnlyIfIsDirty)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->SaveLoadedAsset(AssetToSave, bOnlyIfIsDirty);
}

bool UEditorAssetLibrary::SaveLoadedAssets(const TArray<UObject*>& AssetsToSave, bool bOnlyIfIsDirty)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->SaveLoadedAssets(AssetsToSave, bOnlyIfIsDirty);
}

bool UEditorAssetLibrary::SaveAsset(const FString& AssetsToSave, bool bOnlyIfIsDirty)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->SaveAsset(AssetsToSave, bOnlyIfIsDirty);
}

bool UEditorAssetLibrary::SaveDirectory(const FString& DirectoryPath, bool bOnlyIfIsDirty, bool bRecursive)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->SaveDirectory(DirectoryPath, bOnlyIfIsDirty, bRecursive);
}

/**
 *
 * Directory operations
 *
 **/

bool UEditorAssetLibrary::DoesDirectoryExist(const FString& DirectoryPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DoesDirectoryExist(DirectoryPath);
}

bool UEditorAssetLibrary::DoesDirectoryHaveAssets(const FString& DirectoryPath, bool bRecursive)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->DoesDirectoryContainAssets(DirectoryPath, bRecursive);
}

bool UEditorAssetLibrary::MakeDirectory(const FString& DirectoryPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->MakeDirectory(DirectoryPath);
}

/**
 *
 * List operations
 *
 **/

TArray<FString> UEditorAssetLibrary::ListAssets(const FString& DirectoryPath, bool bRecursive /*= false*/, bool bIncludeFolder /*= false*/)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->ListAssets(DirectoryPath, bRecursive, bIncludeFolder);
}

TArray<FString> UEditorAssetLibrary::ListAssetByTagValue(FName TagName, const FString& TagValue)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->ListAssetsByTagValue(TagName, TagValue);
}

TMap<FName, FString> UEditorAssetLibrary::GetTagValues(const FString& AssetPath)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->GetTagValues(AssetPath);
}

TMap<FName, FString> UEditorAssetLibrary::GetMetadataTagValues(UObject* Object)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->GetMetadataTagValues(Object);
}

FString UEditorAssetLibrary::GetMetadataTag(UObject* Object, FName Tag)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->GetMetadataTag(Object, Tag);
}

void UEditorAssetLibrary::SetMetadataTag(UObject* Object, FName Tag, const FString& Value)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->SetMetadataTag(Object, Tag, Value);
}

void UEditorAssetLibrary::RemoveMetadataTag(UObject* Object, FName Tag)
{
	UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
	return EditorAssetSubsystem->RemoveMetadataTag(Object, Tag);
}

void UEditorAssetLibrary::SyncBrowserToObjects(const TArray<FString>& AssetPaths)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::IsInEditorAndNotPlaying() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return;
	}

	if (GEditor)
	{
		TArray<FAssetData> Assets;
		for (const FString& AssetPath : AssetPaths)
		{
			FString FailureReason;
			FAssetData Asset = EditorScriptingUtils::FindAssetDataFromAnyPath(AssetPath, FailureReason);
			if (Asset.IsValid())
			{
				Assets.Add(Asset);
			}
			else
			{
				UE_LOG(LogEditorScripting, Warning, TEXT("SyncBrowserToObjects. Cannot sync %s : %s"), *AssetPath, *FailureReason);
			}
		}
		if (Assets.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(Assets);
		}
	}
}

#undef LOCTEXT_NAMESPACE


