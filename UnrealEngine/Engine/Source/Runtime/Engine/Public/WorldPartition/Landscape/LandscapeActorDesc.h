// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"

/**
 * ActorDesc for LandscapeActors.
 */
class ENGINE_API FLandscapeActorDesc : public FPartitionActorDesc
{
public:
	virtual void Init(const AActor* InActor) override;
	virtual bool Equals(const FWorldPartitionActorDesc* Other) const override;
	virtual void Unload() override;
	virtual const FGuid& GetSceneOutlinerParent() const override;

protected:
	virtual void Serialize(FArchive& Ar) override;

private:
	FGuid LandscapeActorGuid;
};
#endif
