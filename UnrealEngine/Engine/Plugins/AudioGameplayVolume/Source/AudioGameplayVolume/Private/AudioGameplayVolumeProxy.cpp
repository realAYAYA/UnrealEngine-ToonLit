// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioGameplayVolumeProxy.h"
#include "AudioAnalytics.h"
#include "AudioGameplayVolumeMutator.h"
#include "AudioGameplayVolumeSubsystem.h"
#include "AudioGameplayVolumeLogs.h"
#include "AudioGameplayVolumeComponent.h"
#include "Interfaces/IAudioGameplayCondition.h"
#include "Components/BrushComponent.h"
#include "Components/PrimitiveComponent.h"

namespace AudioGameplayVolumeConsoleVariables
{
	int32 bProxyDistanceCulling = 1;
	FAutoConsoleVariableRef CVarProxyDistanceCulling(
		TEXT("au.AudioGameplayVolumes.PrimitiveProxy.DistanceCulling"),
		bProxyDistanceCulling,
		TEXT("Skips physics body queries for proxies that are not close to the listener.\n0: Disable, 1: Enable (default)"),
		ECVF_Default);

} // namespace AudioGameplayVolumeConsoleVariables

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioGameplayVolumeProxy)

UAudioGameplayVolumeProxy::UAudioGameplayVolumeProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UAudioGameplayVolumeProxy::ContainsPosition(const FVector& Position) const
{ 
	return false;
}

void UAudioGameplayVolumeProxy::InitFromComponent(const UAudioGameplayVolumeComponent* Component)
{
	if (!Component || !Component->GetWorld())
	{
		UE_LOG(AudioGameplayVolumeLog, Verbose, TEXT("AudioGameplayVolumeProxy - Attempted Init from invalid volume component!"));
		return;
	}

	VolumeID = Component->GetUniqueID();
	WorldID = Component->GetWorld()->GetUniqueID();

	PayloadType = PayloadFlags::AGCP_None;
	ProxyVolumeMutators.Reset();

	TInlineComponentArray<UAudioGameplayVolumeMutator*> Components(Component->GetOwner());
	for (UAudioGameplayVolumeMutator* Comp : Components)
	{
		if (!Comp || !Comp->IsActive())
		{
			continue;
		}

		TSharedPtr<FProxyVolumeMutator> NewMutator = Comp->CreateMutator();
		if (NewMutator.IsValid())
		{
			NewMutator->VolumeID = VolumeID;
			NewMutator->WorldID = WorldID;

			AddPayloadType(NewMutator->PayloadType);
			ProxyVolumeMutators.Emplace(NewMutator);
		}
	}

	Audio::Analytics::RecordEvent_Usage(TEXT("AudioGameplayVolume.InitializedFromComponent"));
}

void UAudioGameplayVolumeProxy::FindMutatorPriority(FAudioProxyMutatorPriorities& Priorities) const
{
	check(IsInAudioThread());
	for (const TSharedPtr<FProxyVolumeMutator>& ProxyVolumeMutator : ProxyVolumeMutators)
	{
		if (!ProxyVolumeMutator.IsValid())
		{
			continue;
		}

		ProxyVolumeMutator->UpdatePriority(Priorities);
	}
}

void UAudioGameplayVolumeProxy::GatherMutators(const FAudioProxyMutatorPriorities& Priorities, FAudioProxyMutatorSearchResult& OutResult) const
{
	check(IsInAudioThread());
	for (const TSharedPtr<FProxyVolumeMutator>& ProxyVolumeMutator : ProxyVolumeMutators)
	{
		if (!ProxyVolumeMutator.IsValid())
		{
			continue;
		}

		if (ProxyVolumeMutator->CheckPriority(Priorities))
		{
			ProxyVolumeMutator->Apply(OutResult.InteriorSettings);
			OutResult.MatchingMutators.Add(ProxyVolumeMutator);
		}
	}
}

void UAudioGameplayVolumeProxy::AddPayloadType(PayloadFlags InType)
{
	PayloadType |= InType;
}

bool UAudioGameplayVolumeProxy::HasPayloadType(PayloadFlags InType) const
{
	return (PayloadType & InType) != PayloadFlags::AGCP_None;
}

uint32 UAudioGameplayVolumeProxy::GetVolumeID() const
{ 
	return VolumeID;
}

uint32 UAudioGameplayVolumeProxy::GetWorldID() const
{
	return WorldID;
}

UAGVPrimitiveComponentProxy::UAGVPrimitiveComponentProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UAGVPrimitiveComponentProxy::ContainsPosition(const FVector& Position) const
{
	SCOPED_NAMED_EVENT(UAGVPrimitiveComponentProxy_ContainsPosition, FColor::Blue);

	FBodyInstance* BodyInstancePointer = nullptr;
	if (UPrimitiveComponent* PrimitiveComponent = WeakPrimative.Get())
	{
		if (NeedsPhysicsQuery(PrimitiveComponent, Position))
		{
			BodyInstancePointer = PrimitiveComponent->GetBodyInstance();
		}
	}

	if (!BodyInstancePointer)
	{
		return false;
	}

	float DistanceSquared = 0.f;
	FVector PointOnBody = FVector::ZeroVector;
	return BodyInstancePointer->GetSquaredDistanceToBody(Position, DistanceSquared, PointOnBody) && FMath::IsNearlyZero(DistanceSquared);
}

void UAGVPrimitiveComponentProxy::InitFromComponent(const UAudioGameplayVolumeComponent* Component)
{
	Super::InitFromComponent(Component);

	if (Component)
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Component->GetOwner());
		const int32 PrimitiveComponentCount = PrimitiveComponents.Num();

		if (PrimitiveComponentCount != 1)
		{
			UE_LOG(AudioGameplayVolumeLog, Warning, TEXT("Was expecting exactly one Primitive Component on the owning actor, found %d - this could cause unexpected behavior"), PrimitiveComponentCount);
		}

		if (PrimitiveComponents.Num() > 0)
		{
			WeakPrimative = PrimitiveComponents[0];
		}
	}
}

bool UAGVPrimitiveComponentProxy::NeedsPhysicsQuery(UPrimitiveComponent* PrimitiveComponent, const FVector& Position) const
{
	check(PrimitiveComponent);

	if (!PrimitiveComponent->IsPhysicsStateCreated() || !PrimitiveComponent->HasValidPhysicsState())
	{
		return false;
	}

	// Temporary kill switch for distance culling
	if (AudioGameplayVolumeConsoleVariables::bProxyDistanceCulling == 0)
	{
		return true;
	}

	// Early distance culling
	const float BoundsRadiusSq = FMath::Square(PrimitiveComponent->Bounds.SphereRadius);
	const float DistanceSq = FVector::DistSquared(PrimitiveComponent->Bounds.Origin, Position);

	return DistanceSq <= BoundsRadiusSq;
}

UAGVConditionProxy::UAGVConditionProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UAGVConditionProxy::ContainsPosition(const FVector& Position) const
{
	SCOPED_NAMED_EVENT(UAGVConditionProxy_ContainsPosition, FColor::Blue);

	const UObject* ObjectWithInterface = WeakObject.Get();
	if (ObjectWithInterface && ObjectWithInterface->Implements<UAudioGameplayCondition>())
	{
		return IAudioGameplayCondition::Execute_ConditionMet(ObjectWithInterface)
			|| IAudioGameplayCondition::Execute_ConditionMet_Position(ObjectWithInterface, Position);
	}

	return false;
}

void UAGVConditionProxy::InitFromComponent(const UAudioGameplayVolumeComponent* Component)
{
	Super::InitFromComponent(Component);

	AActor* OwnerActor = Component ? Component->GetOwner() : nullptr;
	if (OwnerActor)
	{
		if (OwnerActor->Implements<UAudioGameplayCondition>())
		{
			WeakObject = MakeWeakObjectPtr(OwnerActor);
		}
		else
		{
			TInlineComponentArray<UActorComponent*> AllComponents(OwnerActor);

			for (UActorComponent* ActorComponent : AllComponents)
			{
				if (ActorComponent && ActorComponent->Implements<UAudioGameplayCondition>())
				{
					WeakObject = MakeWeakObjectPtr(ActorComponent);
					break;
				}
			}
		}
	}
}

