// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "PackedLevelActor/PackedLevelActorTypes.h"
#include "PackedLevelActor/IPackedLevelActorBuilder.h"

class ALevelInstance;

class FPackedLevelActorRecursiveBuilder : public IPackedLevelActorBuilder
{
public:
	static FPackedLevelActorBuilderID BuilderID;

	virtual FPackedLevelActorBuilderID GetID() const override;
	virtual void GetPackClusters(FPackedLevelActorBuilderContext& InContext, AActor* InActor) const override;
	virtual void PackActors(FPackedLevelActorBuilderContext& InContext, APackedLevelActor* InPackingActor, const FPackedLevelActorBuilderClusterID& InClusterID, const TArray<UActorComponent*>& InComponents) const override;
};

class FPackedLevelActorRecursiveBuilderCluster : public FPackedLevelActorBuilderCluster
{
public:
	FPackedLevelActorRecursiveBuilderCluster(FPackedLevelActorBuilderID InBuilderID, ALevelInstance* InLevelInstance);

	virtual bool Equals(const FPackedLevelActorBuilderCluster& InOther) const override;
	virtual uint32 ComputeHash() const override;

	ALevelInstance* LevelInstance = nullptr;
};

#endif