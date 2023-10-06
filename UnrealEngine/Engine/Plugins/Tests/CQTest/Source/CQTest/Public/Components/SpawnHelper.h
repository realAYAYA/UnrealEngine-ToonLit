// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/GCObject.h"

struct CQTEST_API FSpawnHelper
{
	virtual ~FSpawnHelper();

	template <typename ActorType>
	ActorType& SpawnActor(const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters(), UClass* Class = nullptr)
	{
		return SpawnActorAtInWorld<ActorType>(GetWorld(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters, Class);
	}

	template <typename ActorType>
	ActorType& SpawnActorAt(FVector const& Location, FRotator const& Rotation, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters(), UClass* Class = nullptr)
	{
		return SpawnActorAtInWorld<ActorType>(GetWorld(), Location, Rotation, SpawnParameters, Class);
	}

	template <typename ObjectType>
	ObjectType& SpawnObject()
	{
		static_assert(!TIsDerivedFrom<ObjectType, AActor>::IsDerived, "Use SpawnActor to spawn AActors.");
		static_assert(TIsDerivedFrom<ObjectType, UObject>::IsDerived, "Objects must derive from UObject.");
		ObjectType* const Object = NewObject<ObjectType>();
		check(Object != nullptr);
		SpawnedObjects.Add(Object);
		return *Object;
	}

	UWorld& GetWorld();

protected:
	virtual UWorld* CreateWorld() = 0;

	TArray<TWeakObjectPtr<AActor>> SpawnedActors{};
	TArray<TWeakObjectPtr<UObject>> SpawnedObjects{};

	UWorld* GameWorld{ nullptr };

private:
	template <typename ActorType>
	ActorType& SpawnActorAtInWorld(UWorld& World, const FVector& Location, const FRotator& Rotation, const FActorSpawnParameters& SpawnParameters = FActorSpawnParameters(), UClass* Class = nullptr)
	{
		static_assert(TIsDerivedFrom<ActorType, AActor>::IsDerived, "Provided type does not derive from AActor");

		ActorType* const Actor = Class ? World.SpawnActor<ActorType>(Class, Location, Rotation, SpawnParameters) : World.SpawnActor<ActorType>(Location, Rotation, SpawnParameters);

		check(Actor != nullptr);
		SpawnedActors.Add(Actor);
		return *Actor;
	}
};