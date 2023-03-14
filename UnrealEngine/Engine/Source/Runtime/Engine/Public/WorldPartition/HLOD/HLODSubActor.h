// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"
#include "HLODSubActor.generated.h"

USTRUCT()
struct FHLODSubActorDesc
{
	GENERATED_USTRUCT_BODY()

	FHLODSubActorDesc()
	{}

	FHLODSubActorDesc(FGuid InActorGuid, const FActorContainerID& InContainerID)
#if WITH_EDITORONLY_DATA
		: ActorGuid(InActorGuid)
		, ContainerID(InContainerID)
#endif
	{}

#if WITH_EDITORONLY_DATA
	friend bool operator==(const FHLODSubActorDesc& Lhs, const FHLODSubActorDesc& Rhs)
	{
		return Lhs.ActorGuid == Rhs.ActorGuid && Lhs.ContainerID == Rhs.ContainerID;
	}

	friend bool operator!=(const FHLODSubActorDesc& Lhs, const FHLODSubActorDesc& Rhs)
	{
		return !(Lhs == Rhs);
	}

	friend bool operator<(const FHLODSubActorDesc& Lhs, const FHLODSubActorDesc& Rhs)
	{
		if (Lhs.ActorGuid == Rhs.ActorGuid)
		{
			return Lhs.ContainerID.ID < Rhs.ContainerID.ID;
		}

		return Lhs.ActorGuid < Rhs.ActorGuid;
	}
	
	ENGINE_API friend FArchive& operator<<(FArchive& Ar, FHLODSubActorDesc& SubActor);

	UPROPERTY()
	FGuid ActorGuid;

	UPROPERTY()
	FActorContainerID ContainerID;
#endif
};

USTRUCT()
struct FHLODSubActor
{
	GENERATED_USTRUCT_BODY()

	FHLODSubActor()
#if WITH_EDITORONLY_DATA
		: ActorPackage(NAME_None)
		, ActorPath(NAME_None)
		, ContainerPackage(NAME_None)
		, ContainerTransform(FTransform::Identity)
#endif
	{}

	FHLODSubActor(FGuid InActorGuid, FName InActorPackage, FName InActorPath, const FActorContainerID& InContainerID, FName InContainerPackage, const FTransform& InContainerTransform)
#if WITH_EDITORONLY_DATA
		: ActorGuid(InActorGuid)
		, ActorPackage(InActorPackage)
		, ActorPath(InActorPath)
		, ContainerID(InContainerID)
		, ContainerPackage(InContainerPackage)
		, ContainerTransform(InContainerTransform)
#endif
	{}

#if WITH_EDITORONLY_DATA
	friend bool operator==(const FHLODSubActor& Lhs, const FHLODSubActor& Rhs)
	{
		return Lhs.ActorGuid == Rhs.ActorGuid && Lhs.ContainerID == Rhs.ContainerID;
	}

	friend bool operator!=(const FHLODSubActor& Lhs, const FHLODSubActor& Rhs)
	{
		return !(Lhs == Rhs);
	}

	friend bool operator<(const FHLODSubActor& Lhs, const FHLODSubActor& Rhs)
	{
		if (Lhs.ActorGuid == Rhs.ActorGuid)
		{
			return Lhs.ContainerID.ID < Rhs.ContainerID.ID;
		}

		return Lhs.ActorGuid < Rhs.ActorGuid;
	}
		
	UPROPERTY()
	FGuid ActorGuid;
	
	UPROPERTY()
	FName ActorPackage;

	UPROPERTY()
	FName ActorPath;

	UPROPERTY()
	FActorContainerID ContainerID;

	UPROPERTY()
	FName ContainerPackage;

	UPROPERTY()
	FTransform ContainerTransform;
#endif
};
