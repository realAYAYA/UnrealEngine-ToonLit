// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTagCollection.h"

const FAvaTag* UAvaTagCollection::GetTag(const FAvaTagId& InTagId) const
{
	return Tags.Find(InTagId);
}

TArray<FAvaTagId> UAvaTagCollection::GetTagIds() const
{
	TArray<FAvaTagId> TagIds;
	Tags.GetKeys(TagIds);
	return TagIds;
}

FName UAvaTagCollection::GetTagMapName()
{
	return GET_MEMBER_NAME_CHECKED(UAvaTagCollection, Tags);
}
