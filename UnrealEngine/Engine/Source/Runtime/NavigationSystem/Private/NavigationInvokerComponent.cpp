// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationInvokerComponent.h"
#include "NavigationSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavigationInvokerComponent)

UNavigationInvokerComponent::UNavigationInvokerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TileGenerationRadius(3000)
	, TileRemovalRadius(5000)
{
	bAutoActivate = true;
}

void UNavigationInvokerComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	AActor* Owner = GetOwner();
	if (Owner)
	{
		UNavigationSystemV1::RegisterNavigationInvoker(*Owner, TileGenerationRadius, TileRemovalRadius);
	}
}

void UNavigationInvokerComponent::Deactivate()
{
	Super::Deactivate();

	AActor* Owner = GetOwner();
	if (Owner)
	{
		UNavigationSystemV1::UnregisterNavigationInvoker(*Owner);
	}
}

void UNavigationInvokerComponent::RegisterWithNavigationSystem(UNavigationSystemV1& NavSys)
{
	if (IsActive())
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			NavSys.RegisterInvoker(*Owner, TileGenerationRadius, TileRemovalRadius);
		}
	}
}

void UNavigationInvokerComponent::SetGenerationRadii(const float GenerationRadius, const float RemovalRadius)
{
	TileGenerationRadius = GenerationRadius;
	TileRemovalRadius = RemovalRadius;
}

