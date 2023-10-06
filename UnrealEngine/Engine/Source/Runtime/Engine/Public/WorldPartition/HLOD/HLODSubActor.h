// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Guid.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"
#include "HLODSubActor.generated.h"


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
		return Lhs.ActorGuid == Rhs.ActorGuid 
			&& Lhs.ActorPackage == Rhs.ActorPackage
			&& Lhs.ActorPath == Rhs.ActorPath
			&& Lhs.ContainerID == Rhs.ContainerID
			&& Lhs.ContainerPackage == Rhs.ContainerPackage
			&& Lhs.ContainerTransform.Equals(Rhs.ContainerTransform);
	}

	friend bool operator!=(const FHLODSubActor& Lhs, const FHLODSubActor& Rhs)
	{
		return !(Lhs == Rhs);
	}

	friend bool operator<(const FHLODSubActor& Lhs, const FHLODSubActor& Rhs)
	{
		if (Lhs.ActorGuid == Rhs.ActorGuid)
		{
			return Lhs.ContainerID < Rhs.ContainerID;
		}

		return Lhs.ActorGuid < Rhs.ActorGuid;
	}

	friend uint32 GetTypeHash(const FHLODSubActor& Key)
	{
		return HashCombine(GetTypeHash(Key.ActorGuid), GetTypeHash(Key.ContainerID));
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
