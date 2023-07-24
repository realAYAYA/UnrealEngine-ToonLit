// Copyright Epic Games, Inc. All Rights Reserved.
#include "Audio/ActorSoundParameterInterface.h"
#include "AudioParameter.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorSoundParameterInterface)


void UActorSoundParameterInterface::Fill(const AActor* OwningActor, TArray<FAudioParameter>& OutParams)
{
	TArray<const AActor*> Actors;
	TArray<const UActorComponent*> Components;
	GetImplementers(OwningActor, Actors, Components);

	TArray<FAudioParameter> TempParams;

	for (const AActor* Actor : Actors)
	{
		IActorSoundParameterInterface::Execute_GetActorSoundParams(Actor, TempParams);
		OutParams.Append(MoveTemp(TempParams));
	}

	for (const UActorComponent* Component : Components)
	{
		IActorSoundParameterInterface::Execute_GetActorSoundParams(Component, TempParams);
		OutParams.Append(MoveTemp(TempParams));
	}
}

void UActorSoundParameterInterface::GetImplementers(const AActor* InActor, TArray<const AActor*>& OutActors, TArray<const UActorComponent*>& OutComponents)
{
	if (!InActor)
	{
		return;
	}

	// Helper to collect objects that implement this interface from an actor (and its components)
	auto CollectFromActor = [&OutActors, &OutComponents](const AActor* InActor)
	{
		if (InActor)
		{
			if (InActor->Implements<UActorSoundParameterInterface>())
			{
				OutActors.Add(InActor);
			}

			TArray<UActorComponent*> Components = InActor->GetComponentsByInterface(UActorSoundParameterInterface::StaticClass());
			OutComponents.Append(Components);
		}
	};

	// Collect Actors/Components that implement this interface
	const AActor* RootActor = InActor;
	if (USceneComponent* RootComp = RootActor->GetRootComponent())
	{
		// Walk up to the top-most attach actor in the hierarchy (will just be the RootActor if no attachment)
		RootActor = RootComp->GetAttachmentRootActor();
	}

	CollectFromActor(RootActor);

	// Grab all attached actors (recursive)
	TArray<AActor*> AttachedActors;
	constexpr bool bResetArray = false;
	constexpr bool bRecursivelyIncludeAttachedActors = true;
	RootActor->GetAttachedActors(AttachedActors, bResetArray, bRecursivelyIncludeAttachedActors);

	for (const AActor* Actor : AttachedActors)
	{
		CollectFromActor(Actor);
	}
}

