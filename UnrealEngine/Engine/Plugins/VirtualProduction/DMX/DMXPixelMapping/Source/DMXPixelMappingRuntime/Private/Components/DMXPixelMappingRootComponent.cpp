// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingRootComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"


void UDMXPixelMappingRootComponent::BeginDestroy()
{
	Super::BeginDestroy();

	// Explicitly reset the children, needed to clear entangled references in these
	Children.Reset();
}

const FName& UDMXPixelMappingRootComponent::GetNamePrefix()
{
	static FName NamePrefix = TEXT("Root");
	return NamePrefix;
}

void UDMXPixelMappingRootComponent::ResetDMX()
{
	ForEachChild([&](UDMXPixelMappingBaseComponent* InComponent)
	{
		if (UDMXPixelMappingRendererComponent* Component = Cast<UDMXPixelMappingRendererComponent>(InComponent))
		{
			Component->ResetDMX();
		}
	}, false);
}

void UDMXPixelMappingRootComponent::SendDMX()
{
	ForEachChild([&](UDMXPixelMappingBaseComponent* InComponent)
	{
		if (UDMXPixelMappingRendererComponent* Component = Cast<UDMXPixelMappingRendererComponent>(InComponent))
		{
			Component->SendDMX();
		}
	}, false);
}

void UDMXPixelMappingRootComponent::Render()
{
	ForEachChild([&](UDMXPixelMappingBaseComponent* InComponent)
	{
		if (UDMXPixelMappingRendererComponent* Component = Cast<UDMXPixelMappingRendererComponent>(InComponent))
		{
			Component->Render();
		}
	}, false);
}

void UDMXPixelMappingRootComponent::RenderAndSendDMX()
{
	Render();
	SendDMX();
}

bool UDMXPixelMappingRootComponent::CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const
{
	return false;
}
