// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReferencePose.h"
#include "LODPose.h"
#include "AnimNext_LODPose.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "ReferencePose"))
struct FAnimNextGraphReferencePose
{
	GENERATED_BODY()

	FAnimNextGraphReferencePose() = default;

	explicit FAnimNextGraphReferencePose(UE::AnimNext::FDataHandle& InReferencePose)
		: ReferencePose(InReferencePose)
	{
	}

	UE::AnimNext::FDataHandle ReferencePose;
};

USTRUCT(BlueprintType, meta = (DisplayName = "LODPose"))
struct FAnimNextGraphLODPose
{
	GENERATED_BODY()

	FAnimNextGraphLODPose() = default;

	explicit FAnimNextGraphLODPose(const UE::AnimNext::FLODPoseHeap& InLODPose)
		: LODPose(InLODPose)
	{
	}
	explicit FAnimNextGraphLODPose(UE::AnimNext::FLODPoseHeap&& InLODPose)
		: LODPose(MoveTemp(InLODPose))
	{
	}

	UE::AnimNext::FLODPoseHeap LODPose;
};
