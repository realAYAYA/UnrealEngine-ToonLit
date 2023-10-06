// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/ClusterUnionReplicatedProxyComponent.h"

#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/ClusterUnionComponent.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClusterUnionReplicatedProxyComponent)

UClusterUnionReplicatedProxyComponent::UClusterUnionReplicatedProxyComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	ParentClusterUnion = nullptr;
	ChildClusteredComponent = nullptr;

	bNetUpdateParentClusterUnion = false;
	bNetUpdateChildClusteredComponent = false;
	bNetUpdateParticleBoneIds = false;
	bNetUpdateParticleChildToParents = false;
}

void UClusterUnionReplicatedProxyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UActorComponent::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UClusterUnionReplicatedProxyComponent, ParentClusterUnion, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UClusterUnionReplicatedProxyComponent, ChildClusteredComponent, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UClusterUnionReplicatedProxyComponent, ParticleBoneIds, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UClusterUnionReplicatedProxyComponent, ParticleChildToParents, Params);
}

void UClusterUnionReplicatedProxyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UActorComponent::EndPlay(EndPlayReason);

	MarkPendingDeletion();

	if (AActor* Owner = GetOwner(); Owner && DeferSetChildToParentHandle.IsValid())
	{
		Owner->GetWorldTimerManager().ClearTimer(DeferSetChildToParentHandle);
	}

	if (!GetOwner()->HasAuthority() && ParentClusterUnion.IsValid() && ChildClusteredComponent.IsValid())
	{
		ParentClusterUnion->RemoveComponentFromCluster(ChildClusteredComponent.Get());
	}
}

void UClusterUnionReplicatedProxyComponent::SetParentClusterUnion(UClusterUnionComponent* InComponent)
{
	ParentClusterUnion = InComponent;
	MARK_PROPERTY_DIRTY_FROM_NAME(UClusterUnionReplicatedProxyComponent, ParentClusterUnion, this);
}

void UClusterUnionReplicatedProxyComponent::SetChildClusteredComponent(UPrimitiveComponent* InComponent)
{
	ChildClusteredComponent = InComponent;
	MARK_PROPERTY_DIRTY_FROM_NAME(UClusterUnionReplicatedProxyComponent, ChildClusteredComponent, this);
}

void UClusterUnionReplicatedProxyComponent::SetParticleBoneIds(const TArray<int32>& InIds)
{
	ParticleBoneIds = InIds;

	ParticleChildToParents.Empty();
	ParticleChildToParents.Reserve(InIds.Num());
	for (int32 Index = 0; Index < InIds.Num(); ++Index)
	{
		ParticleChildToParents.Add(FTransform::Identity);
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(UClusterUnionReplicatedProxyComponent, ParticleBoneIds, this);
}

void UClusterUnionReplicatedProxyComponent::SetParticleChildToParent(int32 BoneId, const FTransform& ChildToParent)
{
	int32 Index = INDEX_NONE;
	if (ParticleBoneIds.Find(BoneId, Index))
	{
		ParticleChildToParents[Index] = ChildToParent;
		MARK_PROPERTY_DIRTY_FROM_NAME(UClusterUnionReplicatedProxyComponent, ParticleChildToParents, this);
	}
}

void UClusterUnionReplicatedProxyComponent::OnRep_ParentClusterUnion()
{
	bNetUpdateParentClusterUnion = true;
}

void UClusterUnionReplicatedProxyComponent::OnRep_ChildClusteredComponent()
{
	bNetUpdateChildClusteredComponent = true;
}

void UClusterUnionReplicatedProxyComponent::OnRep_ParticleBoneIds()
{
	bNetUpdateParticleBoneIds = true;
}

void UClusterUnionReplicatedProxyComponent::OnRep_ParticleChildToParents()
{
	bNetUpdateParticleChildToParents = true;
}

void UClusterUnionReplicatedProxyComponent::PostRepNotifies()
{
	UActorComponent::PostRepNotifies();

	const bool bIsValid = ParentClusterUnion.IsValid() && ChildClusteredComponent.IsValid() && !ParticleBoneIds.IsEmpty();
	if (!bIsValid)
	{
		return;
	}

	// These three properties should only get set once when the component is created.
	const bool bIsInitialReplication = bNetUpdateParentClusterUnion || bNetUpdateChildClusteredComponent || bNetUpdateParticleBoneIds;
	if (bIsInitialReplication)
	{
		if (!DeferAddComponentToClusterHandle.IsValid())
		{
			DeferAddComponentToClusterHandleUntilInitialTransformUpdate();
		}
		bNetUpdateParentClusterUnion = false;
		bNetUpdateChildClusteredComponent = false;
		bNetUpdateParticleBoneIds = false;
	}

	if (bIsValid && bNetUpdateParticleChildToParents && ParticleBoneIds.Num() == ParticleChildToParents.Num())
	{
		// This particular bit can't happen until *after* we add the component to the cluster union. There's an additional deferral
		// in AddComponentToCluster that we have to wait for.
		if (!DeferSetChildToParentHandle.IsValid())
		{
			DeferSetChildToParentChildUntilClusteredComponentInParentUnion();
		}
		bNetUpdateParticleChildToParents = false;
	}
}

void UClusterUnionReplicatedProxyComponent::DeferAddComponentToClusterHandleUntilInitialTransformUpdate()
{
	if (!ParentClusterUnion.IsValid() || !ChildClusteredComponent.IsValid() || IsPendingDeletion())
	{
		return;
	}

	DeferAddComponentToClusterHandle.Invalidate();

	if (ParentClusterUnion->HasReceivedTransform())
	{
		ParentClusterUnion->AddComponentToCluster(ChildClusteredComponent.Get(), ParticleBoneIds);
	}
	else if (AActor* Owner = GetOwner())
	{
		DeferAddComponentToClusterHandle = Owner->GetWorldTimerManager().SetTimerForNextTick(this, &UClusterUnionReplicatedProxyComponent::DeferAddComponentToClusterHandleUntilInitialTransformUpdate);
	}
}

void UClusterUnionReplicatedProxyComponent::DeferSetChildToParentChildUntilClusteredComponentInParentUnion()
{
	if (!ParentClusterUnion.IsValid() || !ChildClusteredComponent.IsValid() || IsPendingDeletion())
	{
		return;
	}

	DeferSetChildToParentHandle.Invalidate();

	if (ParentClusterUnion->IsComponentAdded(ChildClusteredComponent.Get()))
	{
		ParentClusterUnion->ForceSetChildToParent(ChildClusteredComponent.Get(), ParticleBoneIds, ParticleChildToParents);
	}
	else if (AActor* Owner = GetOwner())
	{
		DeferSetChildToParentHandle = Owner->GetWorldTimerManager().SetTimerForNextTick(this, &UClusterUnionReplicatedProxyComponent::DeferSetChildToParentChildUntilClusteredComponentInParentUnion);
	}
}