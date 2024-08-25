// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tags/AvaTagAttribute.h"

#define LOCTEXT_NAMESPACE "AvaTagAttribute"

FText UAvaTagAttribute::GetDisplayName() const
{
	return FText::Format(LOCTEXT("DisplayName", "Tag Attribute: {0}"), FText::FromName(Tag.ToName()));
}

bool UAvaTagAttribute::ContainsTag(const FAvaTagHandle& InTagHandle) const
{
	return Tag.MatchesTag(InTagHandle);
}

#undef LOCTEXT_NAMESPACE
