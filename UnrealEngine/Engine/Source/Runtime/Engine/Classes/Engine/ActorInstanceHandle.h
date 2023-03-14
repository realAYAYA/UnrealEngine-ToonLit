// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorInstanceHandle.generated.h"

// Handle to a unique object. This may specify a full weigh actor or it may only specify the light weight instance that represents the same object.
USTRUCT(BlueprintType)
struct ENGINE_API FActorInstanceHandle
{
	GENERATED_BODY()

	friend struct FLightWeightInstanceSubsystem;
	friend class ALightWeightInstanceManager;
	friend class UActorInstanceHandleInterface;

	FActorInstanceHandle();

	explicit FActorInstanceHandle(AActor* InActor);
	explicit FActorInstanceHandle(class ALightWeightInstanceManager* Manager, int32 InInstanceIndex);

	FActorInstanceHandle(const FActorInstanceHandle& Other);

	bool IsValid() const;

	bool DoesRepresentClass(const UClass* OtherClass) const;

	UClass* GetRepresentedClass() const;

	FVector GetLocation() const;
	FRotator GetRotation() const;
	FTransform GetTransform() const;

	FName GetFName() const;
	FString GetName() const;

	/** If this handle has a valid actor, return it; otherwise return the actor responsible for managing the instances. */
	AActor* GetManagingActor() const;

	/** Returns either the actor's root component or the root component for the manager associated with the handle */
	USceneComponent* GetRootComponent() const;

	/** Returns the actor specified by this handle. This may require loading and creating the actor object. */
	AActor* FetchActor() const;
	template <typename T>
	T* FetchActor() const;

	/* Returns the index used internally by the manager */
	FORCEINLINE int32 GetInstanceIndex() const { return InstanceIndex; }

	/* Returns the index used by rendering and collision */
	int32 GetRenderingInstanceIndex() const;

	FActorInstanceHandle& operator=(const FActorInstanceHandle& Other) = default;
	FActorInstanceHandle& operator=(FActorInstanceHandle&& Other) = default;
	FActorInstanceHandle& operator=(AActor* OtherActor);

	bool operator==(const FActorInstanceHandle& Other) const;
	bool operator!=(const FActorInstanceHandle& Other) const;

	bool operator==(const AActor* OtherActor) const;
	bool operator!=(const AActor* OtherActor) const;

	friend ENGINE_API uint32 GetTypeHash(const FActorInstanceHandle& Handle);

	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FActorInstanceHandle& Handle);

	uint32 GetInstanceUID() const { return InstanceUID; }

private:
	/**
	 * helper functions that let us treat the actor pointer as a UObject in templated functions
	 * these do NOT fetch the actor so they will return nullptr if we don't have a full actor representation
	 */
	UObject* GetActorAsUObject();
	const UObject* GetActorAsUObject() const;

	/** Returns true if Actor is not null and not pending kill */
	bool IsActorValid() const;

	/** this is cached here for convenience */
	UPROPERTY()
		mutable TWeakObjectPtr<AActor> Actor;

	/** Identifies the light weight instance manager to use */
	TWeakObjectPtr<ALightWeightInstanceManager> Manager;

	/** Identifies the instance within the manager */
	int32 InstanceIndex;

	/** Unique identifier for instances represented by the handle */
	uint32 InstanceUID;
};

template <typename T>
T* FActorInstanceHandle::FetchActor() const
{
	return Cast<T>(FetchActor());
}
