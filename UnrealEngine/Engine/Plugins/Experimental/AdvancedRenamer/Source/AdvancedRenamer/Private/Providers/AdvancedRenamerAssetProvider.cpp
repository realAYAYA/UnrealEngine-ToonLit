// Copyright Epic Games, Inc. All Rights Reserved.

#include "Providers/AdvancedRenamerAssetProvider.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"

#define LOCTEXT_NAMESPACE "AdvancedRenamerAssetProvider"

FAdvancedRenamerAssetProvider::FAdvancedRenamerAssetProvider()
{
}

FAdvancedRenamerAssetProvider::~FAdvancedRenamerAssetProvider()
{
}

void FAdvancedRenamerAssetProvider::SetAssetList(const TArray<FAssetData>& InAssetList)
{
	AssetList.Empty();
	AssetList.Append(InAssetList);
}

void FAdvancedRenamerAssetProvider::AddAssetList(const TArray<FAssetData>& InAssetList)
{
	AssetList.Append(InAssetList);
}

void FAdvancedRenamerAssetProvider::AddAssetData(const FAssetData& InAsset)
{
	AssetList.Add(InAsset);
}

UObject* FAdvancedRenamerAssetProvider::GetAsset(int32 Index) const
{
	if (!AssetList.IsValidIndex(Index))
	{
		return nullptr;
	}

	return AssetList[Index].GetAsset();
}

int32 FAdvancedRenamerAssetProvider::Num() const
{
	return AssetList.Num();
}

bool FAdvancedRenamerAssetProvider::IsValidIndex(int32 Index) const
{
	UObject* Asset = GetAsset(Index);

	return IsValid(Asset) && Asset->IsAsset();
}

FString FAdvancedRenamerAssetProvider::GetOriginalName(int32 Index) const
{
	UObject* Asset = GetAsset(Index);

	if (!IsValid(Asset))
	{
		return "";
	}

	return Asset->GetName();
}

uint32 FAdvancedRenamerAssetProvider::GetHash(int32 Index) const
{
	UObject* Asset = GetAsset(Index);

	if (!IsValid(Asset))
	{
		return 0;
	}

	return GetTypeHash(Asset);
}

bool FAdvancedRenamerAssetProvider::RemoveIndex(int32 Index)
{
	if (!AssetList.IsValidIndex(Index))
	{
		return false;
	}

	AssetList.RemoveAt(Index);
	return true;
}

bool FAdvancedRenamerAssetProvider::CanRename(int32 Index) const
{
	UObject* Asset = GetAsset(Index);

	if (!IsValid(Asset))
	{
		return false;
	}

	return true;
}

bool FAdvancedRenamerAssetProvider::ExecuteRename(int32 Index, const FString& NewName)
{
	UObject* Asset = GetAsset(Index);

	if (!IsValid(Asset))
	{
		return false;
	}

	FString PackagePath = Asset->GetPathName();
	PackagePath = FPaths::GetPath(PackagePath);

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	return AssetTools.RenameAssets({FAssetRenameData(Asset, PackagePath, NewName)});
}

#undef LOCTEXT_NAMESPACE
