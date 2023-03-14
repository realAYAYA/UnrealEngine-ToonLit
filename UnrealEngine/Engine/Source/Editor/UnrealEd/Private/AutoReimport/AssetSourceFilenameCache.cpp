// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoReimport/AssetSourceFilenameCache.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetDataTagMap.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CoreGlobals.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"

FAssetSourceFilenameCache::FAssetSourceFilenameCache()
{
	if (IsEngineExitRequested())
	{
		// This can get created for the first time on shutdown, if so don't do anything
		return;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	AssetRegistry.OnAssetAdded().AddRaw(this, &FAssetSourceFilenameCache::HandleOnAssetAdded);
	AssetRegistry.OnAssetRemoved().AddRaw(this, &FAssetSourceFilenameCache::HandleOnAssetRemoved);
	AssetRegistry.OnAssetRenamed().AddRaw(this, &FAssetSourceFilenameCache::HandleOnAssetRenamed);

	UAssetImportData::OnImportDataChanged.AddRaw(this, &FAssetSourceFilenameCache::HandleOnAssetUpdated);

	TArray<FAssetData> Assets;
	if (AssetRegistry.GetAllAssets(Assets))
	{
		for (auto& Asset : Assets)
		{
			HandleOnAssetAdded(Asset);
		}
	}
}

FAssetSourceFilenameCache& FAssetSourceFilenameCache::Get()
{
	static FAssetSourceFilenameCache Cache;
	return Cache;
}

void FAssetSourceFilenameCache::Shutdown()
{
	FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule)
	{
		IAssetRegistry* AssetRegistry = AssetRegistryModule->TryGet();
		if (AssetRegistry)
		{
			AssetRegistry->OnAssetAdded().RemoveAll(this);
			AssetRegistry->OnAssetRemoved().RemoveAll(this);
			AssetRegistry->OnAssetRenamed().RemoveAll(this);
		}
	}
	
	AssetRenamedEvent.Clear();

	UAssetImportData::OnImportDataChanged.RemoveAll(this);
}

TOptional<FAssetImportInfo> FAssetSourceFilenameCache::ExtractAssetImportInfo(const FAssetData& AssetData)
{
	static const FName LegacySourceFilePathName("SourceFile");

	FAssetDataTagMapSharedView::FFindTagResult Result = AssetData.TagsAndValues.FindTag(UObject::SourceFileTagName());
	if (Result.IsSet())
	{
		return FAssetImportInfo::FromJson(Result.GetValue());
	}
	else
	{
		FAssetDataTagMapSharedView::FFindTagResult ResultLegacy = AssetData.TagsAndValues.FindTag(LegacySourceFilePathName);
		if (ResultLegacy.IsSet())
		{
			FAssetImportInfo Legacy;
			Legacy.Insert(ResultLegacy.GetValue());
			return Legacy;
		}
		else
		{
			return TOptional<FAssetImportInfo>();
		}
	}
}

void FAssetSourceFilenameCache::HandleOnAssetAdded(const FAssetData& AssetData)
{
	TOptional<FAssetImportInfo> ImportData = ExtractAssetImportInfo(AssetData);
	if (ImportData.IsSet())
	{
		for (const auto& SourceFile : ImportData->SourceFiles)
		{
			SourceFileToObjectPathCache.FindOrAdd(FPaths::GetCleanFilename(SourceFile.RelativeFilename)).Add(AssetData.GetSoftObjectPath());
		}
	}
}

void FAssetSourceFilenameCache::HandleOnAssetRemoved(const FAssetData& AssetData)
{
	TOptional<FAssetImportInfo> ImportData = ExtractAssetImportInfo(AssetData);
	if (ImportData.IsSet())
	{
		for (auto& SourceFile : ImportData->SourceFiles)
		{
			FString CleanFilename = FPaths::GetCleanFilename(SourceFile.RelativeFilename);
			if (auto* Objects = SourceFileToObjectPathCache.Find(CleanFilename))
			{
				Objects->Remove(AssetData.GetSoftObjectPath());
				if (Objects->Num() == 0)
				{
					SourceFileToObjectPathCache.Remove(CleanFilename);
				}
			}
		}
	}
}

void FAssetSourceFilenameCache::HandleOnAssetRenamed(const FAssetData& AssetData, const FString& OldPath)
{
	TOptional<FAssetImportInfo> ImportData = ExtractAssetImportInfo(AssetData);
	if (ImportData.IsSet())
	{
		for (auto& SourceFile : ImportData->SourceFiles)
		{
			FString CleanFilename = FPaths::GetCleanFilename(SourceFile.RelativeFilename);

			if (auto* Objects = SourceFileToObjectPathCache.Find(CleanFilename))
			{
				Objects->Remove(FSoftObjectPath(OldPath));
				if (Objects->Num() == 0)
				{
					SourceFileToObjectPathCache.Remove(CleanFilename);
				}
			}

			SourceFileToObjectPathCache.FindOrAdd(CleanFilename).Add(AssetData.GetSoftObjectPath());
		}
	}

	AssetRenamedEvent.Broadcast(AssetData, OldPath);
}

void FAssetSourceFilenameCache::HandleOnAssetUpdated(const FAssetImportInfo& OldData, const UAssetImportData* ImportData)
{
	FSoftObjectPath ObjectPath = FSoftObjectPath(ImportData->GetOuter());

	for (const auto& SourceFile : OldData.SourceFiles)
	{
		FString CleanFilename = FPaths::GetCleanFilename(SourceFile.RelativeFilename);
		if (auto* Objects = SourceFileToObjectPathCache.Find(CleanFilename))
		{
			Objects->Remove(ObjectPath);
			if (Objects->Num() == 0)
			{
				SourceFileToObjectPathCache.Remove(CleanFilename);
			}
		}
	}

	for (const auto& SourceFile : ImportData->SourceData.SourceFiles)
	{
		SourceFileToObjectPathCache.FindOrAdd(FPaths::GetCleanFilename(SourceFile.RelativeFilename)).Add(ObjectPath);
	}
}

TArray<FAssetData> FAssetSourceFilenameCache::GetAssetsPertainingToFile(const IAssetRegistry& Registry, const FString& AbsoluteFilename) const
{
	TArray<FAssetData> Assets;
	
	if (const auto* ObjectPaths = SourceFileToObjectPathCache.Find(FPaths::GetCleanFilename(AbsoluteFilename)))
	{
		for (const FSoftObjectPath& Path : *ObjectPaths)
		{
			FAssetData Asset = Registry.GetAssetByObjectPath(Path);
			TOptional<FAssetImportInfo> ImportInfo = ExtractAssetImportInfo(Asset);
			if (ImportInfo.IsSet())
			{
				auto AssetPackagePath = FPackageName::LongPackageNameToFilename(Asset.PackagePath.ToString() / TEXT(""));

				// Attempt to find the matching source filename in the list of imported sorce files (generally there are only one of these)
				const bool bWasImportedFromFile = ImportInfo->SourceFiles.ContainsByPredicate([&](const FAssetImportInfo::FSourceFile& File){

					if (AbsoluteFilename == FPaths::ConvertRelativePathToFull(AssetPackagePath / File.RelativeFilename) ||
						AbsoluteFilename == FPaths::ConvertRelativePathToFull(File.RelativeFilename))
					{
						return true;
					}

					return false;
				});

				if (bWasImportedFromFile)
				{
					Assets.Emplace();
					Swap(Asset, Assets[Assets.Num() - 1]);
				}
			}
		}
	}

	return Assets;
}
