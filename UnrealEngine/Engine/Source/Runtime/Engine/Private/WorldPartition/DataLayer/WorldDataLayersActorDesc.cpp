// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/WorldDataLayersActorDesc.h"

#if WITH_EDITOR

#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"
#include "UObject/FortniteSeasonBranchObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

FArchive& operator<<(FArchive& Ar, FDataLayerInstanceDesc& Desc)
{
	Ar << Desc.Name << Desc.ParentName << Desc.bIsUsingAsset;

	if (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) < FFortniteSeasonBranchObjectVersion::FixedDataLayerInstanceDesc)
	{
		Ar << Desc.PrivateShortName;
		if (Desc.bIsUsingAsset)
		{
			Ar << Desc.AssetPath << Desc.bDeprecatedIsRuntime;
		}
	}
	else
	{
		if (Desc.bIsUsingAsset)
		{
			Ar << Desc.AssetPath;
		}
		else
		{
			Ar << Desc.bDeprecatedIsRuntime << Desc.PrivateShortName;
		}
	}

	// Fixup redirected data layer asset path
	if (Ar.IsLoading() && Desc.bIsUsingAsset)
	{
		FWorldPartitionHelpers::FixupRedirectedAssetPath(Desc.AssetPath);
	}

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionActorFilter)
	{
		Ar << Desc.bIsIncludedInActorFilterDefault;
	}

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionPrivateDataLayers)
	{
		Ar << Desc.bIsPrivate;

		if (Desc.bIsPrivate)
		{
			Ar << Desc.PrivateShortName;
			Ar << Desc.bPrivateDataLayerSupportsActorFilter;
		}
	}

	return Ar;
}

bool operator == (const FDataLayerInstanceDesc& Lhs, const FDataLayerInstanceDesc& Rhs)
{
	return (Lhs.Name == Rhs.Name) &&
		(Lhs.ParentName == Rhs.ParentName) &&
		(Lhs.bIsUsingAsset == Rhs.bIsUsingAsset) &&
		(Lhs.AssetPath == Rhs.AssetPath) &&
		(Lhs.bDeprecatedIsRuntime == Rhs.bDeprecatedIsRuntime) &&
		(Lhs.PrivateShortName == Rhs.PrivateShortName) &&
		(Lhs.bIsIncludedInActorFilterDefault == Rhs.bIsIncludedInActorFilterDefault) &&
		(Lhs.bIsPrivate == Rhs.bIsPrivate) &&
		(Lhs.bPrivateDataLayerSupportsActorFilter == Rhs.bPrivateDataLayerSupportsActorFilter);
}

bool operator < (const FDataLayerInstanceDesc& Lhs, const FDataLayerInstanceDesc& Rhs)
{
	if (Lhs.Name == Rhs.Name)
	{
		if (Lhs.ParentName == Rhs.ParentName)
		{
			if (Lhs.bIsUsingAsset == Rhs.bIsUsingAsset)
			{
				if (Lhs.AssetPath == Rhs.AssetPath)
				{
					if (Lhs.bDeprecatedIsRuntime == Rhs.bDeprecatedIsRuntime)
					{
						if (Lhs.PrivateShortName == Rhs.PrivateShortName)
						{
							if (Lhs.bIsIncludedInActorFilterDefault == Rhs.bIsIncludedInActorFilterDefault)
							{
								if (Lhs.bIsPrivate == Rhs.bIsPrivate)
								{
									return (int)Lhs.bPrivateDataLayerSupportsActorFilter == (int)Rhs.bPrivateDataLayerSupportsActorFilter;
								}

								return (int)Lhs.bIsPrivate < (int)Rhs.bIsPrivate;
							}
							
							return (int)Lhs.bIsIncludedInActorFilterDefault < (int)Rhs.bIsIncludedInActorFilterDefault;
						}
						return Lhs.PrivateShortName < Rhs.PrivateShortName;
					}
					return (int)Lhs.bDeprecatedIsRuntime < (int)Rhs.bDeprecatedIsRuntime;
				}
				return Lhs.AssetPath.LexicalLess(Rhs.AssetPath);
			}
			return (int)Lhs.bIsUsingAsset < (int)Rhs.bIsUsingAsset;
		}
		return Lhs.ParentName.LexicalLess(Rhs.ParentName);
	}
	return Lhs.Name.LexicalLess(Rhs.Name);
}

UDataLayerAsset* FDataLayerInstanceDesc::GetAsset() const
{
	if (!bIsPrivate && IsUsingAsset() && !AssetPath.IsNone())
	{
		UDataLayerAsset* Asset = LoadObject<UDataLayerAsset>(nullptr, *AssetPath.ToString());
		return Asset;
	}
	return nullptr;
}

EDataLayerType FDataLayerInstanceDesc::GetDataLayerType() const
{
	if (bIsPrivate)
	{
		return EDataLayerType::Editor;
	}

	if (IsUsingAsset())
	{
		if (UDataLayerAsset* DataLayerAsset = GetAsset())
		{
			return DataLayerAsset->IsRuntime() ? EDataLayerType::Runtime : EDataLayerType::Editor;
		}
		return EDataLayerType::Unknown;
	}
	return bDeprecatedIsRuntime ? EDataLayerType::Runtime : EDataLayerType::Editor;
}

FString FDataLayerInstanceDesc::GetShortName() const
{
	if (!bIsPrivate && IsUsingAsset())
	{
		static FString UnknownString(TEXT("Unknown"));
		UDataLayerAsset* DataLayerAsset = GetAsset();
		return DataLayerAsset ? DataLayerAsset->GetName() : UnknownString;
	}
	return PrivateShortName;
}

bool FDataLayerInstanceDesc::SupportsActorFilters() const
{
	if (bIsPrivate)
	{
		return bPrivateDataLayerSupportsActorFilter;
	}

	if (IsUsingAsset())
	{
		if (UDataLayerAsset* DataLayerAsset = GetAsset())
		{
			return DataLayerAsset->SupportsActorFilters();
		}
	}
	
	return false;
}

FDataLayerInstanceDesc::FDataLayerInstanceDesc()
: bIsUsingAsset(false)
, bIsIncludedInActorFilterDefault(true)
, bIsPrivate(false)
, bPrivateDataLayerSupportsActorFilter(false)
, bDeprecatedIsRuntime(false)
{
}

void FDataLayerInstanceDesc::Init(UDataLayerInstance* InDataLayerInstance)
{
	Name = InDataLayerInstance->GetDataLayerFName();
	ParentName = InDataLayerInstance->GetParent() ? InDataLayerInstance->GetParent()->GetDataLayerFName() : NAME_None;
	const UDataLayerInstanceWithAsset* DataLayerWithAsset = Cast<UDataLayerInstanceWithAsset>(InDataLayerInstance);
	const UDataLayerAsset* DataLayerAsset = InDataLayerInstance->GetAsset();
	bIsUsingAsset = InDataLayerInstance->Implements<UDataLayerInstanceWithAsset>() || (DataLayerAsset != nullptr);
	if (bIsUsingAsset && DataLayerAsset)
	{
		AssetPath = FName(DataLayerAsset->GetPathName());
		bIsPrivate = DataLayerAsset->IsPrivate();
	}
	
	bIsIncludedInActorFilterDefault = InDataLayerInstance->IsIncludedInActorFilterDefault();
	bPrivateDataLayerSupportsActorFilter = InDataLayerInstance->SupportsActorFilters();
	PrivateShortName = InDataLayerInstance->GetDataLayerShortName();
	bDeprecatedIsRuntime = InDataLayerInstance->IsRuntime();
}

FWorldDataLayersActorDesc::FWorldDataLayersActorDesc()
: bIsValid(false)
{}

void FWorldDataLayersActorDesc::Init(const AActor* InActor)
{
	FWorldPartitionActorDesc::Init(InActor);

	const AWorldDataLayers* WorldDataLayers = CastChecked<AWorldDataLayers>(InActor);
	WorldDataLayers->ForEachDataLayer([this](UDataLayerInstance* DataLayerInstance)
	{
		FDataLayerInstanceDesc& DataLayerInstanceDesc = DataLayerInstances.Emplace_GetRef();
		DataLayerInstanceDesc.Init(DataLayerInstance);
		return true;
	});

	bIsValid = true;
}

bool FWorldDataLayersActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	if (FWorldPartitionActorDesc::Equals(Other))
	{
		const FWorldDataLayersActorDesc* OtherDesc = (FWorldDataLayersActorDesc*)Other;
		return CompareUnsortedArrays(DataLayerInstances, OtherDesc->DataLayerInstances);
	}
	return false;
}

void FWorldDataLayersActorDesc::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FFortniteSeasonBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	FWorldPartitionActorDesc::Serialize(Ar);

	if (!bIsDefaultActorDesc)
	{
		if (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) >= FFortniteSeasonBranchObjectVersion::AddedWorldDataLayersActorDesc)
		{
			Ar << DataLayerInstances;
			bIsValid = true;
		}
	}
}

const FDataLayerInstanceDesc* FWorldDataLayersActorDesc::GetDataLayerInstanceFromInstanceName(FName InDataLayerInstanceName) const
{
	for (const FDataLayerInstanceDesc& DataLayerInstance : DataLayerInstances)
	{
		if (DataLayerInstance.GetName().IsEqual(InDataLayerInstanceName, ENameCase::CaseSensitive))
		{
			return &DataLayerInstance;
		}
	}
	return nullptr;
}

const FDataLayerInstanceDesc* FWorldDataLayersActorDesc::GetDataLayerInstanceFromAssetPath(FName InDataLayerAssetPath) const
{
	for (const FDataLayerInstanceDesc& DataLayerInstance : DataLayerInstances)
	{
		if (FName(DataLayerInstance.GetAssetPath()) == InDataLayerAssetPath)
		{
			return &DataLayerInstance;
		}
	}
	return nullptr;
}

#endif
