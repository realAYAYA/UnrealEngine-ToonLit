// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

void UTargetTagsGameplayEffectComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// Try to find the parent and update the inherited tags.  We do this 'early' because this is the only function
	// we can use for creation of the object (and this runs post-constructor).
	const UTargetTagsGameplayEffectComponent* Parent = FindParentComponent(*this);
	InheritableGrantedTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableGrantedTagsContainer : nullptr);

#if WITH_EDITORONLY_DATA
	EditorFriendlyName = TEXT("Target Tags (Granted to Actor)");
#endif // WITH_EDITORONLY_DATA
}

void UTargetTagsGameplayEffectComponent::OnGameplayEffectChanged() const
{
	Super::OnGameplayEffectChanged();
	ApplyTargetTagChanges();
}

#if WITH_EDITOR

void UTargetTagsGameplayEffectComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GetInheritableGrantedTagsContainerName())
	{
		SetAndApplyTargetTagChanges(InheritableGrantedTagsContainer);

		// Tell the GE it needs to reconfigure itself based on these updated properties (this will reaggregate the tags)
		UGameplayEffect* Owner = GetOwner();
		Owner->OnGameplayEffectChanged();
	}
}
#endif // WITH_EDITOR

void UTargetTagsGameplayEffectComponent::SetAndApplyTargetTagChanges(const FInheritedTagContainer& TagContainerMods)
{
	InheritableGrantedTagsContainer = TagContainerMods;

	// Try to find the parent and update the inherited tags
	const UTargetTagsGameplayEffectComponent* Parent = FindParentComponent(*this);
	InheritableGrantedTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableGrantedTagsContainer : nullptr);

	// Apply to the owning Gameplay Effect Component
	ApplyTargetTagChanges();
}

void UTargetTagsGameplayEffectComponent::ApplyTargetTagChanges() const
{
	UGameplayEffect* Owner = GetOwner();
	InheritableGrantedTagsContainer.ApplyTo(Owner->CachedGrantedTags);
}
