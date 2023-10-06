// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayEffectComponents/BlockAbilityTagsGameplayEffectComponent.h"

void UBlockAbilityTagsGameplayEffectComponent::PostInitProperties()
{
	Super::PostInitProperties();

	// Try to find the parent and update the inherited tags.  We do this 'early' because this is the only function
	// we can use for creation of the object (and this runs post-constructor).
	const UBlockAbilityTagsGameplayEffectComponent* Parent = FindParentComponent(*this);
	InheritableBlockedAbilityTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableBlockedAbilityTagsContainer : nullptr);

#if WITH_EDITORONLY_DATA
	EditorFriendlyName = TEXT("Block Abilities w/ Tags");
#endif
}

void UBlockAbilityTagsGameplayEffectComponent::OnGameplayEffectChanged() const
{
	Super::OnGameplayEffectChanged();
	ApplyBlockedAbilityTagChanges();
}

#if WITH_EDITOR
void UBlockAbilityTagsGameplayEffectComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GetInheritableBlockedAbilityTagsContainerPropertyName())
	{
		SetAndApplyBlockedAbilityTagChanges(InheritableBlockedAbilityTagsContainer);
		
		// Tell the GE it needs to reconfigure itself based on these updated properties (this will reaggregate the tags)
		UGameplayEffect* Owner = GetOwner();
		Owner->OnGameplayEffectChanged();
	}
}
#endif // WITH_EDITOR

void UBlockAbilityTagsGameplayEffectComponent::SetAndApplyBlockedAbilityTagChanges(const FInheritedTagContainer& TagContainerMods)
{
	InheritableBlockedAbilityTagsContainer = TagContainerMods;

	// Try to find the parent and update the inherited tags
	const UBlockAbilityTagsGameplayEffectComponent* Parent = FindParentComponent(*this);
	InheritableBlockedAbilityTagsContainer.UpdateInheritedTagProperties(Parent ? &Parent->InheritableBlockedAbilityTagsContainer : nullptr);

	ApplyBlockedAbilityTagChanges();
}

void UBlockAbilityTagsGameplayEffectComponent::ApplyBlockedAbilityTagChanges() const
{
	// Apply to the owning Gameplay Effect Component
	UGameplayEffect* Owner = GetOwner();
	InheritableBlockedAbilityTagsContainer.ApplyTo(Owner->CachedBlockedAbilityTags);
}