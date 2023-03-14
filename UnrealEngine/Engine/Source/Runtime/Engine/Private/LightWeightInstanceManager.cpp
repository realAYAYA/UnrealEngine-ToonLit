// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/LightWeightInstanceManager.h"
#include "GameFramework/LightWeightInstanceSubsystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LightWeightInstanceManager)

UActorInstanceHandleInterface::UActorInstanceHandleInterface(const FObjectInitializer& ObjectInitializer)
{
	// do nothing
}

ALightWeightInstanceManager::ALightWeightInstanceManager(const FObjectInitializer& ObjectInitializer)
{
	bReplicates = true;
	AcceptedClass = AActor::StaticClass();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		FLightWeightInstanceSubsystem::Get().LWInstanceManagers.Add(this);
	}
}

ALightWeightInstanceManager::~ALightWeightInstanceManager()
{
	FLightWeightInstanceSubsystem::Get().LWInstanceManagers.Remove(this);
}

void ALightWeightInstanceManager::SetRepresentedClass(UClass* ActorClass)
{
	RepresentedClass = ActorClass;
}

UClass* ALightWeightInstanceManager::GetInterfaceClass() const
{
	return UActorInstanceHandleInterface::StaticClass();
}

void ALightWeightInstanceManager::Tick(float DeltaSeconds)
{
	// do nothing
}

AActor* ALightWeightInstanceManager::FetchActorFromHandle(const FActorInstanceHandle& Handle)
{
	// make sure the handle doesn't have an actor already
	if (ensure(!Handle.Actor.IsValid()))
	{
		// check if we already have an actor for this handle
		AActor* const* FoundActor = nullptr;
		if ((FoundActor = Actors.Find(Handle.GetInstanceIndex())) == nullptr)
		{
			// spawn a new actor
			ConvertInstanceToActor(Handle);
		}
		else
		{
			Handle.Actor = *FoundActor;
		}
	}

	ensure(Handle.Actor.IsValid());
	return Handle.Actor.Get();
}

AActor* ALightWeightInstanceManager::ConvertInstanceToActor(const FActorInstanceHandle& Handle)
{
	// we shouldn't be calling this on indices that already have an actor representing them
	if (Actors.Contains(Handle.GetInstanceIndex()) && Actors[Handle.GetInstanceIndex()] != nullptr)
	{
		return Actors[Handle.GetInstanceIndex()];
	}
	// if we're pointing at an invalid index we can't return an actor
	if (ValidIndices.Num() <= Handle.GetInstanceIndex() || ValidIndices[Handle.GetInstanceIndex()] == false)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SetSpawnParameters(SpawnParams);

	AActor* NewActor = GetLevel()->GetWorld()->SpawnActor<AActor>(GetActorClassToSpawn(Handle), InstanceTransforms[Handle.GetInstanceIndex()], SpawnParams);
	check(NewActor);

	Handle.Actor = NewActor;
	Actors.Add(Handle.GetInstanceIndex(), NewActor);

	PostActorSpawn(Handle);

	return NewActor;
}

int32 ALightWeightInstanceManager::FindIndexForActor(const AActor* InActor) const
{
	for (const TPair<int32, AActor*>& ActorPair : Actors)
	{
		if (ActorPair.Value == InActor)
		{
			return ActorPair.Key;
		}
	}

	return INDEX_NONE;
}

FActorInstanceHandle ALightWeightInstanceManager::ConvertActorToLightWeightInstance(AActor* InActor)
{
	FActorInstanceHandle ReturnHandle;
	if (!InActor)
	{
		return ReturnHandle;
	}

	if (FLWIData* Data = AllocateInitData())
	{
		SetDataFromActor(Data, InActor);
		// If we used to manage this actor as a light weight instance then we will already have entries associated with it. Check for these so we can use them again instead of allocating new entries
		int32 Idx = INDEX_NONE;
		for (auto& IndexActorPair : Actors)
		{
			if (IndexActorPair.Value == InActor)
			{
				Idx = IndexActorPair.Key;
				break;
			}
		}

		if (Idx == INDEX_NONE)
		{
			Idx = AddNewInstance(Data);
			Idx = ConvertInternalIndexToHandleIndex(Idx);
		}
		else
		{
			UpdateDataAtIndex(Data, Idx);
		}

		// Update our handle
		ReturnHandle.Manager = this;
		ReturnHandle.InstanceIndex = Idx;
		ReturnHandle.InstanceUID = InActor->GetWorld()->LWILastAssignedUID++;

		// cleanup
		InActor->Destroy();
		delete Data;
	}
	else
	{
		// something went wrong and we can't manage this actor
		// just return a handle to the actor
		ReturnHandle.Actor = InActor;
	}

	return ReturnHandle;
}

FLWIData* ALightWeightInstanceManager::AllocateInitData() const
{
	return new FLWIData;
}


bool ALightWeightInstanceManager::SetDataFromActor(FLWIData* InData, AActor* InActor) const
{
	if (!InData || !InActor)
	{
		return false;
	}

	InData->Transform = InActor->GetActorTransform();

	return true;
}

int32 ALightWeightInstanceManager::ConvertCollisionIndexToLightWeightIndex(int32 InIndex) const
{
	return InIndex;
}

int32 ALightWeightInstanceManager::ConvertLightWeightIndexToCollisionIndex(int32 InIndex) const
{
	return InIndex;
}

void ALightWeightInstanceManager::SetSpawnParameters(FActorSpawnParameters& SpawnParams)
{
	SpawnParams.OverrideLevel = GetLevel();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags = RF_Transactional;
}

UClass* ALightWeightInstanceManager::GetActorClassToSpawn(const FActorInstanceHandle& Handle) const
{
	return RepresentedClass;
}

void ALightWeightInstanceManager::PostActorSpawn(const FActorInstanceHandle& Handle)
{
	// do nothing
}

bool ALightWeightInstanceManager::IsIndexValid(int32 Index) const
{
	if (ValidIndices.IsValidIndex(Index))
	{
		return ValidIndices[Index];
	}

	return false;
}

bool ALightWeightInstanceManager::FindActorForHandle(const FActorInstanceHandle& Handle) const
{
	ensure(!Handle.Actor.IsValid());

	AActor* const* FoundActor = Actors.Find(Handle.GetInstanceIndex());
	Handle.Actor = FoundActor ? *FoundActor : nullptr;
	return Handle.Actor != nullptr;
}

FVector ALightWeightInstanceManager::GetLocation(const FActorInstanceHandle& Handle) const
{
	if (FindActorForHandle(Handle))
	{
		return Handle.Actor->GetActorLocation();
	}

	if (ensure(IsIndexValid(Handle.GetInstanceIndex())))
	{
		return InstanceTransforms[Handle.GetInstanceIndex()].GetTranslation();
	}

	return FVector();
}

FRotator ALightWeightInstanceManager::GetRotation(const FActorInstanceHandle& Handle) const
{
	if (FindActorForHandle(Handle))
	{
		return Handle.Actor->GetActorRotation();
	}

	if (ensure(IsIndexValid(Handle.GetInstanceIndex())))
	{
		return InstanceTransforms[Handle.GetInstanceIndex()].Rotator();
	}

	return FRotator();
}

FTransform ALightWeightInstanceManager::GetTransform(const FActorInstanceHandle& Handle) const
{
	if (FindActorForHandle(Handle))
	{
		return Handle.Actor->GetActorTransform();
	}

	if (ensure(IsIndexValid(Handle.GetInstanceIndex())))
	{
		return InstanceTransforms[Handle.GetInstanceIndex()];
	}

	return FTransform();
}

FString ALightWeightInstanceManager::GetName(const FActorInstanceHandle& Handle) const
{
	if (FindActorForHandle(Handle))
	{
		return Handle.Actor->GetName();
	}

	return FString::Printf(TEXT("%s_%u"), *BaseInstanceName, Handle.GetInstanceIndex());
}

bool ALightWeightInstanceManager::DoesRepresentClass(const UClass* OtherClass) const
{
	return OtherClass ? OtherClass->IsChildOf(RepresentedClass) : false;
}

bool ALightWeightInstanceManager::DoesAcceptClass(const UClass* OtherClass) const
{
	return OtherClass ? OtherClass->IsChildOf(AcceptedClass) : false;
}

UClass* ALightWeightInstanceManager::GetRepresentedClass() const
{
	return RepresentedClass;
}

UClass* ALightWeightInstanceManager::GetAcceptedClass() const
{
	return AcceptedClass;
}

int32 ALightWeightInstanceManager::AddNewInstance(FLWIData* InitData)
{
	if (!InitData)
	{
		return INDEX_NONE;
	}

	// use one of the free indices if any are available; otherwise grow the size of the array
	const int32 DataIdx = FreeIndices.Num() > 0 ? FreeIndices.Pop(false) : ValidIndices.Num();
	
	// Update the rest of our per instance data
	AddNewInstanceAt(InitData, DataIdx);

	return DataIdx;
}

void ALightWeightInstanceManager::AddNewInstanceAt(FLWIData* InitData, int32 Index)
{
	// allocate space on the end of the array if we need to
	if (Index >= ValidIndices.Num())
	{
		GrowDataArrays();
	}
	ensure(Index < ValidIndices.Num());

	// update our data
	UpdateDataAtIndex(InitData, Index);
	ValidIndices[Index] = true;
}

void ALightWeightInstanceManager::GrowDataArrays()
{
	InstanceTransforms.AddUninitialized();
	ValidIndices.AddUninitialized();

	check(InstanceTransforms.Num() == ValidIndices.Num());
}

void ALightWeightInstanceManager::UpdateDataAtIndex(FLWIData* InData, int32 Index)
{
	InstanceTransforms[Index] = InData->Transform;
}

void ALightWeightInstanceManager::RemoveInstance(const int32 Index)
{
	if (ensure(IsIndexValid(Index)))
	{
		// mark the index as no longer in use
		FreeIndices.Add(Index);
		ValidIndices[Index] = false;

		// destroy the associated actor if one existed
		if (AActor** FoundActor = Actors.Find(Index))
		{
			AActor* ActorToDestroy = *FoundActor;
			ActorToDestroy->Destroy();
		}
	}
}

void ALightWeightInstanceManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALightWeightInstanceManager, RepresentedClass);
	DOREPLIFETIME(ALightWeightInstanceManager, InstanceTransforms);
	DOREPLIFETIME(ALightWeightInstanceManager, FreeIndices);
	DOREPLIFETIME(ALightWeightInstanceManager, ValidIndices);
}

void ALightWeightInstanceManager::OnRep_Transforms()
{
	// do nothing
}
#if WITH_EDITOR
void ALightWeightInstanceManager::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (Name == GET_MEMBER_NAME_CHECKED(ALightWeightInstanceManager, RepresentedClass))
	{
		SetRepresentedClass(RepresentedClass);
	}
}
#endif
