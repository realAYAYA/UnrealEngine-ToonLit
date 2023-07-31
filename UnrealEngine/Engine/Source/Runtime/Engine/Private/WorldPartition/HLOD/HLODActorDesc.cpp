// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/HLODActorDesc.h"

#if WITH_EDITOR
#include "Algo/Transform.h"
#include "Algo/RemoveIf.h"
#include "HAL/FileManager.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODLayer.h"

void FHLODActorDesc::Init(const AActor* InActor)
{
	FWorldPartitionActorDesc::Init(InActor);

	const AWorldPartitionHLOD* HLODActor = CastChecked<AWorldPartitionHLOD>(InActor);

	HLODSubActors.Reserve(HLODActor->GetSubActors().Num());
	Algo::Transform(HLODActor->GetSubActors(), HLODSubActors, [](const FHLODSubActor& SubActor) { return FHLODSubActorDesc(SubActor.ActorGuid, SubActor.ContainerID); });

	SourceCellName = HLODActor->GetSourceCellName();
	SourceHLODLayerName = HLODActor->GetSubActorsHLODLayer()->GetFName();
	HLODStats = HLODActor->GetStats();
}

void FHLODActorDesc::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	FWorldPartitionActorDesc::Serialize(Ar);

	if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::WorldPartitionHLODActorDescSerializeHLODSubActors)
	{
		TArray<FGuid> SubActors;
		Ar << SubActors;
	}
	else
	{
		Ar << HLODSubActors;
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
		Ar << SourceCellName;
		Ar << SourceHLODLayerName;
		Ar << HLODStats;

		// Update package size stat on load
		if (Ar.IsLoading())
		{
			HLODStats.Add(FWorldPartitionHLODStats::MemoryDiskSizeBytes, GetPackageSize());
		}
	}
}

bool FHLODActorDesc::Equals(const FWorldPartitionActorDesc* Other) const
{
	if (FWorldPartitionActorDesc::Equals(Other))
	{
		const FHLODActorDesc& HLODActorDesc = *(FHLODActorDesc*)Other;
		return SourceCellName == HLODActorDesc.SourceCellName &&
			   SourceHLODLayerName == HLODActorDesc.SourceHLODLayerName &&
			   HLODStats.OrderIndependentCompareEqual(HLODActorDesc.GetStats()) &&
			   CompareUnsortedArrays(HLODSubActors, HLODActorDesc.HLODSubActors);
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
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(GetActorPackage().ToString(), FPackageName::GetAssetPackageExtension());
	return ::GetPackageSize(PackageFileName);
}

int64 FHLODActorDesc::GetPackageSize(const AWorldPartitionHLOD* InHLODActor)
{
	const FString PackageFileName = InHLODActor->GetPackage()->GetLoadedPath().GetLocalFullPath();
	return ::GetPackageSize(PackageFileName);
}

#endif
