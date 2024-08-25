// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/DataLayer/ExternalDataLayerHelper.h"
#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetRegistry/ARFilter.h"
#include "ExternalPackageHelper.h"
#include "UObject/Object.h"
#include "UObject/AssetRegistryTagsContext.h"
#endif

FString FExternalDataLayerHelper::GetExternalStreamingObjectPackageName(const UExternalDataLayerAsset* InExternalDataLayerAsset)
{
	check(InExternalDataLayerAsset);
	return FString::Printf(TEXT("StreamingObject_%X"), (uint32)InExternalDataLayerAsset->GetUID());
}

FString FExternalDataLayerHelper::GetExternalStreamingObjectName(const UExternalDataLayerAsset* InExternalDataLayerAsset)
{
	check(InExternalDataLayerAsset);
	return SlugStringForValidName(InExternalDataLayerAsset->GetName() + TEXT("_") + InExternalDataLayerAsset->GetUID().ToString() + TEXT("_ExternalStreamingObject"));
}

bool FExternalDataLayerHelper::BuildExternalDataLayerRootPath(const FString& InEDLMountPoint, const FExternalDataLayerUID& InExternalDataLayerUID, FString& OutExternalDataLayerRootPath)
{
	if (InEDLMountPoint.IsEmpty() || !InExternalDataLayerUID.IsValid())
	{
		return false;
	}

	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> Builder;
	Builder += TEXT("/");
	Builder += InEDLMountPoint;
	Builder += GetExternalDataLayerFolder();
	Builder += InExternalDataLayerUID.ToString();
	OutExternalDataLayerRootPath = *Builder;
	return true;
}

FString FExternalDataLayerHelper::GetExternalDataLayerLevelRootPath(const UExternalDataLayerAsset* InExternalDataLayerAsset, const FString& InLevelPackagePath)
{
	check(InExternalDataLayerAsset);
	check(InExternalDataLayerAsset->GetUID().IsValid());
	return FExternalDataLayerHelper::GetExternalDataLayerLevelRootPath(FPackageName::GetPackageMountPoint(InExternalDataLayerAsset->GetPackage()->GetName()).ToString(), InExternalDataLayerAsset->GetUID(), InLevelPackagePath);
}

FString FExternalDataLayerHelper::GetExternalDataLayerLevelRootPath(const FString& InExternalDataLayerMountPoint, const FExternalDataLayerUID& InExternalDataLayerUID, const FString& InLevelPackagePath)
{
	FString ExternalDataLayerRootPath;
	verify(BuildExternalDataLayerRootPath(InExternalDataLayerMountPoint, InExternalDataLayerUID, ExternalDataLayerRootPath));
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> Builder;
	Builder += ExternalDataLayerRootPath;
	Builder += TEXT("/");
	Builder += InLevelPackagePath;
	FString Result = *Builder;
	FPaths::RemoveDuplicateSlashes(Result);
	return Result;
}

#if WITH_EDITOR

static FName GetExternalDataLayerUIDsAssetRegistryTag()
{
	static const FName ExternalDataLayerUIDsTag("ExternalDataLayerUIDs");
	return ExternalDataLayerUIDsTag;
}

void FExternalDataLayerHelper::AddAssetRegistryTags(FAssetRegistryTagsContext OutContext, const TArray<FExternalDataLayerUID>& InExternalDataLayerUIDs)
{
	if (InExternalDataLayerUIDs.Num() > 0)
	{
		FString ExternalDataLayerUIDsStr = FString::JoinBy(InExternalDataLayerUIDs, TEXT(","), [&](const FExternalDataLayerUID& ExternalDataLayerUID) { return ExternalDataLayerUID.ToString(); });
		OutContext.AddTag(UObject::FAssetRegistryTag(GetExternalDataLayerUIDsAssetRegistryTag(), ExternalDataLayerUIDsStr, UObject::FAssetRegistryTag::TT_Hidden));
	}
}

void FExternalDataLayerHelper::GetExternalDataLayerUIDs(const FAssetData& Asset, TArray<FExternalDataLayerUID>& OutExternalDataLayerUIDs)
{
	FString ExternalDataLayerUIDsStr;
	if (Asset.GetTagValue(GetExternalDataLayerUIDsAssetRegistryTag(), ExternalDataLayerUIDsStr))
	{
		TArray<FString> ExternalDataLayerUIDStrArray;
		ExternalDataLayerUIDsStr.ParseIntoArray(ExternalDataLayerUIDStrArray, TEXT(","));
		for (const FString& ExternalDataLayerUIDStr : ExternalDataLayerUIDStrArray)
		{
			FExternalDataLayerUID ExternalDataLayerUID;
			if (FExternalDataLayerUID::Parse(ExternalDataLayerUIDStr, ExternalDataLayerUID))
			{
				OutExternalDataLayerUIDs.Add(ExternalDataLayerUID);
			}
		}
	}
}

void FExternalDataLayerHelper::ForEachExternalDataLayerLevelPackagePath(const FString& InLevelPackageName, TFunctionRef<void(const FString&)> Func)
{
	UClass* GameFeatureDataClass = FindObject<UClass>(nullptr, TEXT("/Script/GameFeatures.GameFeatureData"));
	if (GameFeatureDataClass)
	{
		FARFilter Filter;
		Filter.bIncludeOnlyOnDiskAssets = false;
		Filter.ClassPaths = { GameFeatureDataClass->GetClassPathName() };
		Filter.bRecursivePaths = true;
		TArray<FAssetData> AssetsData;
		FExternalPackageHelper::GetSortedAssets(Filter, AssetsData);

		for (const FAssetData& AssetData : AssetsData)
		{
			const FString MountPoint = FPackageName::GetPackageMountPoint(AssetData.PackagePath.ToString()).ToString();

			TArray<FExternalDataLayerUID> ExternalDataLayerUIDs;
			FExternalDataLayerHelper::GetExternalDataLayerUIDs(AssetData, ExternalDataLayerUIDs);
			for (const FExternalDataLayerUID& ExternalDataLayerUID : ExternalDataLayerUIDs)
			{
				if (ExternalDataLayerUID.IsValid())
				{
					FString LevelPackageEDLPath = FExternalDataLayerHelper::GetExternalDataLayerLevelRootPath(MountPoint, ExternalDataLayerUID, InLevelPackageName);
					Func(LevelPackageEDLPath);
				}
			}
		}
	}
}

bool FExternalDataLayerHelper::IsExternalDataLayerPath(FStringView InExternalDataLayerPath, FExternalDataLayerUID* OutExternalDataLayerUID)
{
	int32 ExternalDataLayerFolderIdx = UE::String::FindFirst(InExternalDataLayerPath, GetExternalDataLayerFolder(), ESearchCase::IgnoreCase);
	if (ExternalDataLayerFolderIdx != INDEX_NONE)
	{
		FStringView RelativeExternalDataLayerPath = InExternalDataLayerPath.RightChop(ExternalDataLayerFolderIdx + GetExternalDataLayerFolder().Len());
		int32 ExternalDataLayerUIDEndIdx = UE::String::FindFirst(RelativeExternalDataLayerPath, TEXT("/"), ESearchCase::IgnoreCase);
		if (ExternalDataLayerUIDEndIdx != INDEX_NONE)
		{
			if (RelativeExternalDataLayerPath.RightChop(ExternalDataLayerUIDEndIdx + 1).Len() > 0) // + 1 to remove the "/"
			{
				FExternalDataLayerUID UID;
				const FString ExternalDataLayerUIDStr = FString(RelativeExternalDataLayerPath.Mid(0, ExternalDataLayerUIDEndIdx));
				return FExternalDataLayerUID::Parse(ExternalDataLayerUIDStr, OutExternalDataLayerUID ? *OutExternalDataLayerUID : UID);
			}
		}
	}
	return false;
}

#endif
