// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorInstanceHandle.generated.h"

// Handle to a unique object. This may specify a full weigh actor or it may only specify the light weight instance that represents the same object.
USTRUCT(BlueprintType)
struct FActorInstanceHandle
{
	GENERATED_BODY()

	friend struct FLightWeightInstanceSubsystem;
	friend class ALightWeightInstanceManager;
	friend class UActorInstanceHandleInterface;

	ENGINE_API FActorInstanceHandle();

	ENGINE_API explicit FActorInstanceHandle(AActor* InActor);
	ENGINE_API explicit FActorInstanceHandle(class ALightWeightInstanceManager* Manager, int32 InInstanceIndex);

	ENGINE_API FActorInstanceHandle(const FActorInstanceHandle& Other);

	ENGINE_API bool IsValid() const;

	ENGINE_API bool DoesRepresentClass(const UClass* OtherClass) const;

	template<typename T>
	bool DoesRepresent() const;

	ENGINE_API UClass* GetRepresentedClass() const;

	ENGINE_API FVector GetLocation() const;
	ENGINE_API FRotator GetRotation() const;
	ENGINE_API FTransform GetTransform() const;

	ENGINE_API FName GetFName() const;
	ENGINE_API FString GetName() const;

	/** If this handle has a valid actor, return it; otherwise return the actor responsible for managing the instances. */
	ENGINE_API AActor* GetManagingActor() const;

	/** Returns either the actor's root component or the root component for the manager associated with the handle */
	ENGINE_API USceneComponent* GetRootComponent() const;

	/** Returns the actor specified by this handle. This may require loading and creating the actor object. */
	ENGINE_API AActor* FetchActor() const;
	template <typename T>
	T* FetchActor() const;

	/* Returns the index used internally by the manager */
	FORCEINLINE int32 GetInstanceIndex() const { return InstanceIndex; }

	/* Returns the index used by rendering and collision */
	ENGINE_API int32 GetRenderingInstanceIndex() const;

	FActorInstanceHandle& operator=(const FActorInstanceHandle& Other) = default;
	FActorInstanceHandle& operator=(FActorInstanceHandle&& Other) = default;
	ENGINE_API FActorInstanceHandle& operator=(AActor* OtherActor);

	ENGINE_API bool operator==(const FActorInstanceHandle& Other) const;
	ENGINE_API bool operator!=(const FActorInstanceHandle& Other) const;

	ENGINE_API bool operator==(const AActor* OtherActor) const;
	ENGINE_API bool operator!=(const AActor* OtherActor) const;

	friend ENGINE_API uint32 GetTypeHash(const FActorInstanceHandle& Handle);

	friend ENGINE_API FArchive& operator<<(FArchive& Ar, FActorInstanceHandle& Handle);

	uint32 GetInstanceUID() const { return InstanceUID; }

private:
	/**
	 * helper functions that let us treat the actor pointer as a UObject in templated functions
	 * these do NOT fetch the actor so they will return nullptr if we don't have a full actor representation
	 */
	ENGINE_API UObject* GetActorAsUObject();
	ENGINE_API const UObject* GetActorAsUObject() const;

	/** Returns true if Actor is not null and not pending kill */
	ENGINE_API bool IsActorValid() const;

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

template<typename T>
bool FActorInstanceHandle::DoesRepresent() const
{
	if (const UClass* RepresentedClass = GetRepresentedClass())
	{
		if constexpr (TIsIInterface<T>::Value)
		{
			return RepresentedClass->ImplementsInterface(T::UClassType::StaticClass());
		}
		else
		{
			return RepresentedClass->IsChildOf(T::StaticClass());
		}
	}
	return false;
}

template <typename T>
T* FActorInstanceHandle::FetchActor() const
{
	if (DoesRepresent<T>())
	{
		return CastChecked<T>(FetchActor(), ECastCheckedType::NullAllowed);
	}
	return nullptr;
}
