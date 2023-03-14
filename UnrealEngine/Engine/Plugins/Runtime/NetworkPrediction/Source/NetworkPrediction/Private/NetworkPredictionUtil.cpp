// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionUtil.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "NetworkPredictionLog.h"

namespace UE_NP
{
	Chaos::FSingleParticlePhysicsProxy* FindBestPhysicsProxy(AActor* Owner, FName NamedComponent)
	{
		// Test is component is valid for Network Physics. Needs a valid physics ActorHandle
		auto ValidComponent = [](UActorComponent* Component)
		{
			bool bValid = false;
			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component))
			{
				bValid = FPhysicsInterface::IsValid(PrimComp->BodyInstance.ActorHandle);
			}
			return bValid;
		};

		auto SelectComponent = [&ValidComponent](const TArray<UActorComponent*>& Components)
		{
			UPrimitiveComponent* Pc = nullptr;
			for (UActorComponent* Ac : Components)
			{
				if (ValidComponent(Ac))
				{
					Pc = (UPrimitiveComponent*)Ac;
					break;
				}

			}
			return Pc;
		};

		Chaos::FSingleParticlePhysicsProxy* Proxy = nullptr;
		UPrimitiveComponent* PrimitiveComponent = nullptr;
		
		// Explicitly tagged component
		if (NamedComponent != NAME_None)
		{
			if (UPrimitiveComponent* FoundComponent = SelectComponent(Owner->GetComponentsByTag(UPrimitiveComponent::StaticClass(), NamedComponent)))
			{
				PrimitiveComponent = FoundComponent;
			}
			else
			{
				UE_LOG(LogNetworkPrediction, Warning, TEXT("Actor %s: could not find a valid Primitive Component with Tag %s"), *Owner->GetPathName(), *NamedComponent.ToString());
			}
		}

		// Root component
		if (!PrimitiveComponent && ValidComponent(Owner->GetRootComponent()))
		{
			PrimitiveComponent = CastChecked<UPrimitiveComponent>(Owner->GetRootComponent());
		}

		// Any other valid primitive component?
		if (!PrimitiveComponent)
		{
			if (UPrimitiveComponent* FoundComponent = SelectComponent(Owner->K2_GetComponentsByClass(UPrimitiveComponent::StaticClass())))
			{
				PrimitiveComponent = FoundComponent;
			}
		}

		if (PrimitiveComponent)
		{
			Proxy = PrimitiveComponent->BodyInstance.ActorHandle;
		}
		return Proxy;
	}
}