// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/WorldDataLayersActorDesc.h"

#if WITH_EDITOR

#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"
#include "UObject/FortniteNCBranchObjectVersion.h"

FArchive& operator<<(FArchive& Ar, FDataLayerInstanceDesc& Desc)
{
	Ar << Desc.Name << Desc.ParentName << Desc.bIsUsingAsset;

	if (Ar.CustomVer(FFortniteNCBranchObjectVersion::GUID) < FFortniteNCBranchObjectVersion::FixedDataLayerInstanceDesc)
	{
		Ar << Desc.ShortName;
		if (Desc.bIsUsingAsset)
		{
			Ar << Desc.AssetPath << Desc.bIsRuntime;
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
			Ar << Desc.bIsRuntime << Desc.ShortName;
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
		(Lhs.bIsRuntime == Rhs.bIsRuntime) &&
		(Lhs.ShortName == Rhs.ShortName);
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
					if (Lhs.bIsRuntime == Rhs.bIsRuntime)
					{
						return Lhs.ShortName < Rhs.ShortName;
					}
					return (int)Lhs.bIsRuntime < (int)Rhs.bIsRuntime;
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
	if (IsUsingAsset() && !AssetPath.IsNone())
	{
		UDataLayerAsset* Asset = LoadObject<UDataLayerAsset>(nullptr, *AssetPath.ToString());
		return Asset;
	}
	return nullptr;
}

EDataLayerType FDataLayerInstanceDesc::GetDataLayerType() const
{
	if (IsUsingAsset())
	{
		if (UDataLayerAsset* DataLayerAsset = GetAsset())
		{
			return DataLayerAsset->IsRuntime() ? EDataLayerType::Runtime : EDataLayerType::Editor;
		}
		return EDataLayerType::Unknown;
	}
	return bIsRuntime ? EDataLayerType::Runtime : EDataLayerType::Editor;
}

FString FDataLayerInstanceDesc::GetShortName() const
{
	if (IsUsingAsset())
	{
		static FString UnknownString(TEXT("Unknown"));
		UDataLayerAsset* DataLayerAsset = GetAsset();
		return DataLayerAsset ? DataLayerAsset->GetName() : UnknownString;
	}
	return ShortName;
}

FDataLayerInstanceDesc::FDataLayerInstanceDesc()
: bIsUsingAsset(false)
, bIsRuntime(false)
{
}

void FDataLayerInstanceDesc::Init(UDataLayerInstance* InDataLayerInstance)
{
	Name = InDataLayerInstance->GetDataLayerFName();
	ParentName = InDataLayerInstance->GetParent() ? InDataLayerInstance->GetParent()->GetDataLayerFName() : NAME_None;
	const UDataLayerInstanceWithAsset* DataLayerWithAsset = Cast<UDataLayerInstanceWithAsset>(InDataLayerInstance);
	bIsUsingAsset = (DataLayerWithAsset != nullptr);
	if (bIsUsingAsset)
	{
		if (const UDataLayerAsset* DataLayerAsset = DataLayerWithAsset->GetAsset())
		{
			AssetPath = FName(DataLayerAsset->GetPathName());
		}
	}
	ShortName = InDataLayerInstance->GetDataLayerShortName();
	bIsRuntime = InDataLayerInstance->IsRuntime();
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
	Ar.UsingCustomVersion(FFortniteNCBranchObjectVersion::GUID);

	FWorldPartitionActorDesc::Serialize(Ar);

	if (Ar.CustomVer(FFortniteNCBranchObjectVersion::GUID) >= FFortniteNCBranchObjectVersion::AddedWorldDataLayersActorDesc)
	{
		Ar << DataLayerInstances;
		bIsValid = true;
	}
}

bool FWorldDataLayersActorDesc::IsRuntimeRelevant(const FActorContainerID& InContainerID) const
{
	return InContainerID.IsMainContainer();
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