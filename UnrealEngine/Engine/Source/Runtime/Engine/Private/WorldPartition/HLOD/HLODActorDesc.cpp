// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODActorDesc.h"
#include "UObject/Package.h"
#include "WorldPartition/HLOD/HLODStats.h"

#if WITH_EDITOR

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODSourceActorsFromCell.h"


void FHLODActorDesc::Init(const AActor* InActor)
{
	FWorldPartitionActorDesc::Init(InActor);

	const UWorldPartition* WorldPartition = InActor->GetWorld() ? InActor->GetWorld()->GetWorldPartition() : nullptr;
	const AWorldPartitionHLOD* HLODActor = CastChecked<AWorldPartitionHLOD>(InActor);
	const UWorldPartitionHLODSourceActors* HLODSourceActors = HLODActor->GetSourceActors();
	const UWorldPartitionHLODSourceActorsFromCell* HLODCellSourceActors = Cast<UWorldPartitionHLODSourceActorsFromCell>(HLODSourceActors);

	if (HLODCellSourceActors && WorldPartition)
	{
		for (const FHLODSubActor& SubActor : HLODCellSourceActors->GetActors())
		{
			if (SubActor.ContainerID.IsMainContainer())
			{
				const FWorldPartitionActorDesc* SubActorDesc = WorldPartition->GetActorDesc(SubActor.ActorGuid);
				if (SubActorDesc && SubActorDesc->GetActorNativeClass()->IsChildOf<AWorldPartitionHLOD>())
				{
					ChildHLODActors.Add(SubActor.ActorGuid);
				}
			}
		}
	}

	if (HLODSourceActors)
	{
		SourceHLODLayerName = NAME_None;
		if (const UHLODLayer* SourceHLODLayer = HLODSourceActors->GetHLODLayer())
		{
			SourceHLODLayerName = SourceHLODLayer->GetFName();
		}
	}
	
	HLODStats = HLODActor->GetStats();
}

struct FHLODSubActorDescDeprecated
{
	friend FArchive& operator<<(FArchive& Ar, FHLODSubActorDescDeprecated& SubActor);
	FGuid ActorGuid;
	FActorContainerID ContainerID;
};

FArchive& operator<<(FArchive& Ar, FHLODSubActorDescDeprecated& SubActor)
{
	Ar << SubActor.ActorGuid;

	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionFActorContainerIDu64ToGuid)
	{
		uint64 ContainerID;
		Ar << ContainerID;
	}
	else
	{
		Ar << SubActor.ContainerID;
	}

	return Ar;
}

void FHLODActorDesc::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	FWorldPartitionActorDesc::Serialize(Ar);

	if (!bIsDefaultActorDesc)
	{
		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::WorldPartitionHLODActorDescSerializeHLODSubActors)
		{
			TArray<FGuid> SubActors;
			Ar << SubActors;
		}
		else if(Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionHLODSourceActorsRefactor)
		{
			TArray<FHLODSubActorDescDeprecated> HLODSubActors;
			Ar << HLODSubActors;
		}
		else
		{
			Ar << ChildHLODActors;
		}
	
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::WorldPartitionHLODActorDescSerializeHLODLayer)
		{
			FString HLODLayer_Deprecated;
			Ar << HLODLayer_Deprecated;
		}

		const bool bSerializeCellHash = Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::WorldPartitionHLODActorDescSerializeCellHash &&
										Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionHLODActorDescSerializeStats;
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::WorldPartitionHLODActorDescSerializeCellHash)
		{
			uint64 CellHash_Deprecated;
			Ar << CellHash_Deprecated;
		}

		if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionActorDescSerializeActorIsRuntimeOnly)
		{
			bActorIsRuntimeOnly = true;
		}

		if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::WorldPartitionHLODActorDescSerializeStats)
		{
			if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WorldPartitionHLODActorUseSourceCellGuid)
			{
				FName SourceCellName;
				Ar << SourceCellName;
			}

			Ar << SourceHLODLayerName;
			Ar << HLODStats;

			// Update package size stat on load
			if (Ar.IsLoading())
			{
				HLODStats.Add(FWorldPartitionHLODStats::MemoryDiskSizeBytes, GetPackageSize());
			}
		}
	}
}

bool FHLODActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	if (FWorldPartitionActorDesc::Equals(Other))
	{
		const FHLODActorDesc& HLODActorDesc = *(FHLODActorDesc*)Other;
		return SourceHLODLayerName == HLODActorDesc.SourceHLODLayerName &&
			   HLODStats.OrderIndependentCompareEqual(HLODActorDesc.GetStats()) &&
			   CompareUnsortedArrays(ChildHLODActors, HLODActorDesc.ChildHLODActors);
	}
	return false;
}

static int64 GetPackageSize(const FString& InPackageFileName)
{
	const int64 PackageSize = IFileManager::Get().FileSize(*InPackageFileName);
	return PackageSize;
}

int64 FHLODActorDesc::GetPackageSize() const
{
	FString PackageFileName = GetActorPackage().ToString();
	FPackageName::TryConvertLongPackageNameToFilename(GetActorPackage().ToString(), PackageFileName, FPackageName::GetAssetPackageExtension());
	return ::GetPackageSize(PackageFileName);
}

int64 FHLODActorDesc::GetPackageSize(const AWorldPartitionHLOD* InHLODActor)
{
	const FString PackageFileName = InHLODActor->GetPackage()->GetLoadedPath().GetLocalFullPath();
	return ::GetPackageSize(PackageFileName);
}

#endif
