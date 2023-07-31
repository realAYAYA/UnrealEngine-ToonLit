// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PackedLevelActor/PackedLevelActorTypes.h"

class APackedLevelActor;
class AActor;
class UActorComponent;
class FPackedLevelActorBuilderContext;

class ENGINE_API IPackedLevelActorBuilder
{
public:
	IPackedLevelActorBuilder() {}
	virtual ~IPackedLevelActorBuilder() {}
	virtual FPackedLevelActorBuilderID GetID() const = 0;
	virtual void GetPackClusters(FPackedLevelActorBuilderContext& InContext, AActor* InActor) const = 0;
	virtual void PackActors(FPackedLevelActorBuilderContext& InBuilder, APackedLevelActor* InPackingActor, const FPackedLevelActorBuilderClusterID& InClusterID, const TArray<UActorComponent*>& InComponents) const = 0;
};

#endif