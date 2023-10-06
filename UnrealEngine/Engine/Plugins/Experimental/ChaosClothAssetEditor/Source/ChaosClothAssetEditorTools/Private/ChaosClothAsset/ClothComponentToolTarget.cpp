// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothComponentToolTarget.h"

#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ComponentReregisterContext.h"
#include "Materials/Material.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothComponentToolTarget)


bool UClothComponentToolTarget::IsValid() const
{
	if (!UPrimitiveComponentToolTarget::IsValid())
	{
		return false;
	}

	const UChaosClothComponent* const ClothComponent = Cast<UChaosClothComponent>(Component);
	if (ClothComponent == nullptr)
	{
		return false;
	}

	const UChaosClothAsset* const ClothAsset = ClothComponent->GetClothAsset();
	if (!ClothAsset || !IsValidChecked(ClothAsset) || ClothAsset->IsUnreachable() || !ClothAsset->IsValidLowLevel())
	{
		return false;
	}

	return true;
}

UChaosClothAsset* UClothComponentToolTarget::GetClothAsset() const
{
	return IsValid() ? Cast<UChaosClothComponent>(Component)->GetClothAsset() : nullptr;
}

UChaosClothComponent* UClothComponentToolTarget::GetClothComponent() const
{
	return IsValid() ? Cast<UChaosClothComponent>(Component) : nullptr;
}


// Factory

bool UClothComponentToolTargetFactory::CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements) const
{
	const UChaosClothComponent* const Component = GetValid(Cast<UChaosClothComponent>(SourceObject));
	return Component && !Component->IsUnreachable() && Component->IsValidLowLevel() && Requirements.AreSatisfiedBy(UClothComponentToolTarget::StaticClass());
}

UToolTarget* UClothComponentToolTargetFactory::BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& Requirements)
{
	UClothComponentToolTarget* const Target = NewObject<UClothComponentToolTarget>(); // TODO: Should we set an outer here?
	Target->InitializeComponent(Cast<UChaosClothComponent>(SourceObject));

	ensure(Target->Component.IsValid() && Requirements.AreSatisfiedBy(Target));

	return Target;
}
