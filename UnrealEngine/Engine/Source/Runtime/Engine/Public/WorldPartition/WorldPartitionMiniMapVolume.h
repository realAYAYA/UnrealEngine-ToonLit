// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "WorldPartitionMiniMapVolume.generated.h"

UCLASS(hidecategories = (Actor, Advanced, Display, Events, Object, Attachment, Info, Input, Blueprint, Layers, Tags, Replication), MinimalAPI)
class AWorldPartitionMiniMapVolume : public AVolume
{
	GENERATED_BODY()

public:
	ENGINE_API AWorldPartitionMiniMapVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const final { return true; }

#if WITH_EDITOR
	virtual bool ActorTypeSupportsDataLayer() const final { return false; }
	virtual bool ActorTypeSupportsExternalDataLayer() const final { return false; }
	virtual bool CanChangeIsSpatiallyLoadedFlag() const { return false; }
#endif
};
