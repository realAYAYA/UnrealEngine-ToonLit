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

	explicit FAnimNextGraphReferencePose(const UE::AnimNext::FReferencePose* InReferencePose)
		: ReferencePose(InReferencePose)
	{
	}

	const UE::AnimNext::FReferencePose* ReferencePose = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "LODPose"))
struct FAnimNextGraphLODPose
{
	GENERATED_BODY()

	FAnimNextGraphLODPose() = default;

	explicit FAnimNextGraphLODPose(const UE::AnimNext::FLODPose& InLODPose)
		: LODPose(InLODPose)
	{
	}
	explicit FAnimNextGraphLODPose(UE::AnimNext::FLODPose&& InLODPose)
		: LODPose(MoveTemp(InLODPose))
	{
	}

	UE::AnimNext::FLODPose LODPose;
};
