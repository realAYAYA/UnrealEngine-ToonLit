// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tags/AvaTagContainerAttribute.h"

#define LOCTEXT_NAMESPACE "AvaTagContainerAttribute"

FText UAvaTagContainerAttribute::GetDisplayName() const
{
	return FText::Format(LOCTEXT("DisplayName", "Tag Container Attribute: {0}"), FText::FromString(TagContainer.ToString()));
}

bool UAvaTagContainerAttribute::ContainsTag(const FAvaTagHandle& InTagHandle) const
{
	return TagContainer.ContainsTag(InTagHandle);
}

void UAvaTagContainerAttribute::SetTagContainer(const FAvaTagHandleContainer& InTagContainer)
{
	TagContainer = InTagContainer;
}

#undef LOCTEXT_NAMESPACE
