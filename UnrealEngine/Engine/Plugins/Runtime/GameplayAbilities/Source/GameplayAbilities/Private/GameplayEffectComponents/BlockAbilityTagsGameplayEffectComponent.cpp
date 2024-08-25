// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayEffectComponents/BlockAbilityTagsGameplayEffectComponent.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "BlockAbilityTagsGameplayEffectComponent"

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

void UBlockAbilityTagsGameplayEffectComponent::OnGameplayEffectChanged()
{
	Super::OnGameplayEffectChanged();
	SetAndApplyBlockedAbilityTagChanges(InheritableBlockedAbilityTagsContainer);
}

#if WITH_EDITOR
void UBlockAbilityTagsGameplayEffectComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GetInheritableBlockedAbilityTagsContainerPropertyName())
	{
		// Tell the GE it needs to reconfigure itself based on these updated properties (this will reaggregate the tags)
		UGameplayEffect* Owner = GetOwner();
		Owner->OnGameplayEffectChanged();
	}
}

EDataValidationResult UBlockAbilityTagsGameplayEffectComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const bool bInstantEffect = (GetOwner()->DurationPolicy == EGameplayEffectDurationType::Instant);
	if (bInstantEffect && !InheritableBlockedAbilityTagsContainer.CombinedTags.IsEmpty())
	{
		Context.AddError(FText::FormatOrdered(LOCTEXT("GEInstantAndBlockAbilityTags", "GE {0} is set to Instant so {1} will not be able to function as expected."), FText::FromString(GetNameSafe(GetOwner())), FText::FromString(EditorFriendlyName)));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
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

#undef LOCTEXT_NAMESPACE