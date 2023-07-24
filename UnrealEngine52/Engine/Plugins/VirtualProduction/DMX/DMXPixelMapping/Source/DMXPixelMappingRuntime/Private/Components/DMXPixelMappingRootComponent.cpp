// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMXPixelMappingRootComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"


void UDMXPixelMappingRootComponent::PostLoad()
{
	Super::PostLoad();

	ForEachChildOfClass<UDMXPixelMappingRendererComponent>([this](UDMXPixelMappingRendererComponent* RendererComponent)
		{
			CachedRendererComponentsByName.Add(RendererComponent->GetFName(), RendererComponent);
		}, false);

	GetOnComponentRenamed().AddUObject(this, &UDMXPixelMappingRootComponent::OnComponentRenamed);
}

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

void UDMXPixelMappingRootComponent::AddChild(UDMXPixelMappingBaseComponent* InComponent)
{
	Super::AddChild(InComponent);

	if (UDMXPixelMappingRendererComponent* RendererComponent = Cast<UDMXPixelMappingRendererComponent>(InComponent))
	{
		CachedRendererComponentsByName.FindOrAdd(InComponent->GetFName(), RendererComponent);
	}
}

void UDMXPixelMappingRootComponent::RemoveChild(UDMXPixelMappingBaseComponent* InComponent)
{
	Super::RemoveChild(InComponent);

	CachedRendererComponentsByName.Remove(InComponent->GetFName());
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

UDMXPixelMappingRendererComponent* UDMXPixelMappingRootComponent::FindRendererComponentByName(const FName& Name) const
{
	if (const TObjectPtr<UDMXPixelMappingRendererComponent>* RendererComponentPtr = CachedRendererComponentsByName.Find(Name))
	{
		return *RendererComponentPtr;
	}

	return nullptr;
}

void UDMXPixelMappingRootComponent::OnComponentRenamed(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* RenamedComponent, UObject* OldOuter, const FName OldName)
{
	ForEachChildOfClass<UDMXPixelMappingRendererComponent>([this](UDMXPixelMappingRendererComponent* RendererComponent)
		{
			CachedRendererComponentsByName.Add(RendererComponent->GetFName(), RendererComponent);
		}, false);
}
