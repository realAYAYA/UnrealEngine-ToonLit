// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/ActorInstanceHandle.h"
#include "OverlapResult.generated.h"

/** Structure containing information about one hit of an overlap test */
USTRUCT()
struct ENGINE_API FOverlapResult
{
	GENERATED_BODY()

	UPROPERTY()
	FActorInstanceHandle OverlapObjectHandle;

	/** PrimitiveComponent that the check hit. */
	UPROPERTY()
	TWeakObjectPtr<class UPrimitiveComponent> Component;

	/** This is the index of the overlapping item.
		For DestructibleComponents, this is the ChunkInfo index.
		For SkeletalMeshComponents this is the Body index or INDEX_NONE for single body */
	int32 ItemIndex;

	/** Utility to return the Actor that owns the Component that was hit */
	AActor* GetActor() const;

	/** Utility to return the Component that was hit */
	UPrimitiveComponent* GetComponent() const;

	/** Indicates if this hit was requesting a block - if false, was requesting a touch instead */
	UPROPERTY()
	uint32 bBlockingHit : 1;

	FOverlapResult()
	{
		FMemory::Memzero(this, sizeof(FOverlapResult));
	}
};

// All members of FOverlapResult are PODs.
template<> struct TIsPODType<FOverlapResult> { enum { Value = true }; };

inline AActor* FOverlapResult::GetActor() const
{
	return OverlapObjectHandle.FetchActor();
}

inline UPrimitiveComponent* FOverlapResult::GetComponent() const
{
	return Component.Get();
}
