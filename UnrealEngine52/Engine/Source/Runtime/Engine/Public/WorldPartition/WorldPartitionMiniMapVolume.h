// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "WorldPartitionMiniMapVolume.generated.h"

UCLASS(hidecategories = (Actor, Advanced, Display, Events, Object, Attachment, Info, Input, Blueprint, Layers, Tags, Replication))
class ENGINE_API AWorldPartitionMiniMapVolume : public AVolume
{
	GENERATED_BODY()

public:
	AWorldPartitionMiniMapVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const final { return true; }

#if WITH_EDITOR
	virtual bool ActorTypeSupportsDataLayer() const final { return false; }
	virtual bool CanChangeIsSpatiallyLoadedFlag() const { return false; }
#endif
};