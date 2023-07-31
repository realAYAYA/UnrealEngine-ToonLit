// Copyright Epic Games, Inc. All Rights Reserved.

#include "ToolTargets/PrimitiveComponentToolTarget.h"

#include "Components/PrimitiveComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PrimitiveComponentToolTarget)

bool UPrimitiveComponentToolTarget::IsValid() const
{
	return Component && ::IsValid(Component) && !Component->IsUnreachable() && Component->IsValidLowLevel();
}

UPrimitiveComponent* UPrimitiveComponentToolTarget::GetOwnerComponent() const
{
	return IsValid() ? Component : nullptr;
}

AActor* UPrimitiveComponentToolTarget::GetOwnerActor() const
{
	return IsValid() ? Component->GetOwner() : nullptr;
}

void UPrimitiveComponentToolTarget::SetOwnerVisibility(bool bVisible) const
{
	if (IsValid())
	{
		Component->SetVisibility(bVisible);
	}
}

FTransform UPrimitiveComponentToolTarget::GetWorldTransform() const
{
	return IsValid() ? Component->GetComponentTransform() : FTransform::Identity;
}

bool UPrimitiveComponentToolTarget::HitTestComponent(const FRay& WorldRay, FHitResult& OutHit) const
{
	FVector End = WorldRay.PointAt(HALF_WORLD_MAX);
	if (IsValid() && Component->LineTraceComponent(OutHit, WorldRay.Origin, End, FCollisionQueryParams(SCENE_QUERY_STAT(HitTest), true)))
	{
		return true;
	}
	return false;
}


// Factory

bool UPrimitiveComponentToolTargetFactory::CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements) const
{
	UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(SourceObject);
	return Component 
		&& ::IsValid(Component)
		&& !Component->IsUnreachable() 
		&& Component->IsValidLowLevel() 
		&& Requirements.AreSatisfiedBy(UPrimitiveComponentToolTarget::StaticClass());
}

UToolTarget* UPrimitiveComponentToolTargetFactory::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements)
{
	UPrimitiveComponentToolTarget* Target = NewObject<UPrimitiveComponentToolTarget>();
	Target->Component = Cast<UPrimitiveComponent>(SourceObject);
	check(Target->Component && Requirements.AreSatisfiedBy(Target));
	return Target;
}



