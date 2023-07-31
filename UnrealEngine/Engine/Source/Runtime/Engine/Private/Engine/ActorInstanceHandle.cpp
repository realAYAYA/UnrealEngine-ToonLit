// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/ActorInstanceHandle.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/LightWeightInstanceSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorInstanceHandle)

FActorInstanceHandle::FActorInstanceHandle()
	: Actor(nullptr)
	, InstanceIndex(INDEX_NONE)
	, InstanceUID(0)
{
	// do nothing
}

FActorInstanceHandle::FActorInstanceHandle(AActor* InActor)
	: Actor(InActor)
	, InstanceIndex(INDEX_NONE)
	, InstanceUID(0)
{
	if (InActor)
	{
#if WITH_EDITOR
		// use the first data layer the actor is in if it's in multiple layers
		TArray<const UDataLayerInstance*> DataLayerInstances = InActor->GetDataLayerInstances();
		const UDataLayerInstance* DataLayerInstance = DataLayerInstances.Num() > 0 ? DataLayerInstances[0] : nullptr;
#else
		const UDataLayerInstance* DataLayerInstance = nullptr;
#endif // WITH_EDITOR
		Manager = FLightWeightInstanceSubsystem::Get().FindLightWeightInstanceManager(InActor->StaticClass(), DataLayerInstance);
	}
}

FActorInstanceHandle::FActorInstanceHandle(ALightWeightInstanceManager* InManager, int32 InInstanceIndex)
	: Actor(nullptr)
	, InstanceIndex(InInstanceIndex)
	, InstanceUID(0)
{
	Manager = InManager;
	if (Manager.IsValid())
	{
		InstanceIndex = Manager->ConvertCollisionIndexToLightWeightIndex(InInstanceIndex);
		if (AActor* const* FoundActor = Manager->Actors.Find(InstanceIndex))
		{
			Actor = *FoundActor;
		}

		UWorld* World = Manager->GetWorld();
		if (ensure(World))
		{
			InstanceUID = World->LWILastAssignedUID++;
		}
	}
}

FActorInstanceHandle::FActorInstanceHandle(const FActorInstanceHandle& Other)
{
	Actor = Other.Actor;

	Manager = Other.Manager;
	InstanceIndex = Other.InstanceIndex;
	InstanceUID = Other.InstanceUID;
}

bool FActorInstanceHandle::IsValid() const
{
	return (Manager.IsValid() && InstanceIndex != INDEX_NONE) || IsActorValid();
}

bool FActorInstanceHandle::DoesRepresentClass(const UClass* OtherClass) const
{
	if (OtherClass == nullptr)
	{
		return false;
	}

	if (IsActorValid())
	{
		return Actor->IsA(OtherClass);
	}

	if (Manager.IsValid())
	{
		return Manager->DoesRepresentClass(OtherClass);
	}

	return false;
}

UClass* FActorInstanceHandle::GetRepresentedClass() const
{
	if (!IsValid())
	{
		return nullptr;
	}

	if (IsActorValid())
	{
		return Actor->GetClass();
	}

	if (Manager.IsValid())
	{
		return Manager->GetRepresentedClass();
	}

	return nullptr;
}

FVector FActorInstanceHandle::GetLocation() const
{
	if (IsActorValid())
	{
		return Actor->GetActorLocation();
	}

	if (Manager.IsValid())
	{
		Manager->GetLocation(*this);
	}

	return FVector();
}

FRotator FActorInstanceHandle::GetRotation() const
{
	if (IsActorValid())
	{
		return Actor->GetActorRotation();
	}

	if (Manager.IsValid())
	{
		Manager->GetRotation(*this);
	}

	return FRotator();
}

FTransform FActorInstanceHandle::GetTransform() const
{
	if (IsActorValid())
	{
		return Actor->GetActorTransform();
	}

	if (Manager.IsValid())
	{
		Manager->GetTransform(*this);
	}

	return FTransform();
}

FName FActorInstanceHandle::GetFName() const
{
	if (IsActorValid())
	{
		return Actor->GetFName();
	}

	return NAME_None;
}

FString FActorInstanceHandle::GetName() const
{
	if (IsActorValid())
	{
		return Actor->GetName();
	}

	if (Manager.IsValid())
	{
		Manager->GetName(*this);
	}

	return FString();
}

AActor* FActorInstanceHandle::GetManagingActor() const
{
	if (IsActorValid())
	{
		return Actor.Get();
	}

	return Manager.Get();
}

USceneComponent* FActorInstanceHandle::GetRootComponent() const
{
	if (IsActorValid())
	{
		return Actor->GetRootComponent();
	}

	return Manager.IsValid() ? Manager->GetRootComponent() : nullptr;
}

AActor* FActorInstanceHandle::FetchActor() const
{
	if (IsActorValid())
	{
		return Actor.Get();
	}

	return FLightWeightInstanceSubsystem::Get().FetchActor(*this);
}

int32 FActorInstanceHandle::GetRenderingInstanceIndex() const
{
	return Manager.IsValid() ? Manager->ConvertLightWeightIndexToCollisionIndex(InstanceIndex) : INDEX_NONE;
}

UObject* FActorInstanceHandle::GetActorAsUObject()
{
	if (IsActorValid())
	{
		return Cast<UObject>(Actor.Get());
	}

	return nullptr;
}

const UObject* FActorInstanceHandle::GetActorAsUObject() const
{
	if (IsActorValid())
	{
		return Cast<UObject>(Actor.Get());
	}

	return nullptr;
}

bool FActorInstanceHandle::IsActorValid() const
{
	return Actor.IsValid();
}

FActorInstanceHandle& FActorInstanceHandle::operator=(AActor* OtherActor)
{
	Actor = OtherActor;
	Manager.Reset();
	InstanceIndex = INDEX_NONE;

	return *this;
}

bool FActorInstanceHandle::operator==(const FActorInstanceHandle& Other) const
{
	// try to compare managers and indices first if we have them
	if (Manager.IsValid() && Other.Manager.IsValid() && InstanceIndex != INDEX_NONE && Other.InstanceIndex != INDEX_NONE)
	{
		return Manager == Other.Manager && InstanceIndex == Other.InstanceIndex;
	}

	// try to compare the actors
	const AActor* MyActor = FetchActor();
	const AActor* OtherActor = Other.FetchActor();

	return MyActor == OtherActor;
}

bool FActorInstanceHandle::operator!=(const FActorInstanceHandle& Other) const
{
	return !(*this == Other);
}

bool FActorInstanceHandle::operator==(const AActor* OtherActor) const
{
	// if we have an actor, compare the two actors
	if (Actor.IsValid())
	{
		return Actor.Get() == OtherActor;
	}

	// if OtherActor is null then we're only equal if this doesn't refer to a valid instance
	if (OtherActor == nullptr)
	{
		return !Manager.IsValid() && InstanceIndex == INDEX_NONE;
	}

	// we don't have an actor so see if we can look up an instance associated with OtherActor and see if we refer to the same instance

#if WITH_EDITOR
	// use the first layer the actor is in if it's in multiple layers
	TArray<const UDataLayerInstance*> DataLayerInstances = OtherActor->GetDataLayerInstances();
	const UDataLayerInstance* DataLayerInstance = DataLayerInstances.Num() > 0 ? DataLayerInstances[0] : nullptr;
#else
	const UDataLayerInstance* DataLayerInstance = nullptr;
#endif // WITH_EDITOR

	if (ALightWeightInstanceManager* LWIManager = FLightWeightInstanceSubsystem::Get().FindLightWeightInstanceManager(OtherActor->StaticClass(), DataLayerInstance))
	{
		if (Manager.Get() != LWIManager)
		{
			return false;
		}

		return Manager->FindIndexForActor(OtherActor) == InstanceIndex;
	}

	return false;
}

bool FActorInstanceHandle::operator!=(const AActor* OtherActor) const
{
	return !(*this == OtherActor);
}

uint32 GetTypeHash(const FActorInstanceHandle& Handle)
{
	uint32 Hash = 0;
	if (Handle.Actor.IsValid())
	{
		FCrc::StrCrc32(*(Handle.Actor->GetPathName()), Hash);
	}
	if (Handle.Manager.IsValid())
	{
		Hash = HashCombine(Hash, GetTypeHash(Handle.Manager.Get()));
	}
	Hash = HashCombine(Hash, Handle.InstanceIndex);

	return Hash;
}

FArchive& operator<<(FArchive& Ar, FActorInstanceHandle& Handle)
{
	Ar << Handle.Actor;
	Ar << Handle.Manager;
	Ar << Handle.InstanceIndex;

	return Ar;
}

