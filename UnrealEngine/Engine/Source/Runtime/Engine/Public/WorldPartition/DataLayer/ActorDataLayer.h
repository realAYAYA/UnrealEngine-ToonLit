// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WorldPartition/DataLayer/DataLayerInstance.h"

#include "ActorDataLayer.generated.h"

// This class is deprecated and only present for backward compatibility purposes.
// Instead of using FActorDatalayer, directly save the DataLayerInstance FName if the DataLayer not exposed in data.
// If the DataLayer is exposed in Data, then use DataLayerAssets.
USTRUCT(BlueprintType)
struct FActorDataLayer
{
	GENERATED_USTRUCT_BODY()

	static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "FActorDataLayer is deprecated and needs to be deleted.");

	FActorDataLayer()
	: Name(NAME_None)
	{}

	FActorDataLayer(const FName& InName)
	: Name(InName)
	{}

	FORCEINLINE bool operator==(const FActorDataLayer& Other) const { return Name == Other.Name; }
	FORCEINLINE bool operator<(const FActorDataLayer& Other) const { return Name.FastLess(Other.Name); }

	FORCEINLINE operator FName () const { return Name; }

	/** The name of this layer */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = DataLayer)
	FName Name;

	friend uint32 GetTypeHash(const FActorDataLayer& Value)
	{
		return GetTypeHash(Value.Name);
	}
};
