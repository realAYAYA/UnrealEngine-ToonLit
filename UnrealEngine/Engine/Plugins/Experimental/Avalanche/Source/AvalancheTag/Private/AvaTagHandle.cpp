// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTagHandle.h"
#include "AvaTagCollection.h"

const FAvaTag* FAvaTagHandle::GetTag() const
{
	if (::IsValid(Source))
	{
		return Source->GetTag(TagId);
	}
	return nullptr;
}

FString FAvaTagHandle::ToString() const
{
	if (const FAvaTag* Tag = GetTag())
	{
		return Tag->ToString();
	}
	return FString();
}

FString FAvaTagHandle::ToDebugString() const
{
	return FString::Printf(TEXT("TagId: %s, Source: %s"), *TagId.ToString(), *GetNameSafe(Source));
}

FName FAvaTagHandle::ToName() const
{
	if (const FAvaTag* Tag = GetTag())
	{
		return Tag->TagName;
	}
	return NAME_None;
}

void FAvaTagHandle::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsSaving() && TagId.IsValid())
	{
		Ar.MarkSearchableName(FAvaTagId::StaticStruct(), *TagId.ToString());
	}
}

bool FAvaTagHandle::MatchesTag(const FAvaTagHandle& InOther) const
{
	const FAvaTag* ThisTag  = GetTag();
	const FAvaTag* OtherTag = InOther.GetTag();

	if (ThisTag && OtherTag)
	{
		return *ThisTag == *OtherTag;
	}

	return MatchesExact(InOther);
}

bool FAvaTagHandle::MatchesExact(const FAvaTagHandle& InOther) const
{
	return Source == InOther.Source && TagId == InOther.TagId;
}
