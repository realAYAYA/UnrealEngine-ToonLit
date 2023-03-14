// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionHandle.h"
#include "Misc/TVariant.h"

class UActorDescContainer;
class AWorldDataLayers;
struct FActorSpawnParameters;

class ENGINE_API FWorldDataLayersReference
{
public:
	FWorldDataLayersReference();
	FWorldDataLayersReference(UActorDescContainer* Container, FName WorldDataLayerName); // Retrieve Actor
	FWorldDataLayersReference(const FActorSpawnParameters& SpawnParameters); // Retrieve and Create Actor if Not Found, FActorSpawnParameters::Name must be set
	FWorldDataLayersReference(FWorldDataLayersReference&& Other);

	~FWorldDataLayersReference();

	bool IsValid() const { return Get() != nullptr; }

	AWorldDataLayers* operator->() { return const_cast<AWorldDataLayers*>(const_cast<const FWorldDataLayersReference*>(this)->operator->()); }
	const AWorldDataLayers* operator->() const { return Get(); }

	const AWorldDataLayers* Get() const;
	AWorldDataLayers* Get() { return const_cast<AWorldDataLayers*>(const_cast<const FWorldDataLayersReference*>(this)->Get()); }

	void Reset();

	FWorldDataLayersReference& operator=(FWorldDataLayersReference&& Other);

private:
	bool TrySetReference(UActorDescContainer* Container, FName WorldDataLayerName);

	TVariant<AWorldDataLayers*, FWorldPartitionReference> WorldDataLayersVariant;
};

#endif // WITH_EDITOR