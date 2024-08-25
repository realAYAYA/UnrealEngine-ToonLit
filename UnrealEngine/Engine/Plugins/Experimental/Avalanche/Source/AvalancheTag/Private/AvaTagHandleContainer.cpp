// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTagHandleContainer.h"
#include "AvaTagCollection.h"
#include "AvaTagHandle.h"

FAvaTagHandleContainer::FAvaTagHandleContainer(const FAvaTagHandle& InTagHandle)
	: Source(InTagHandle.Source)
	, TagIds({ InTagHandle.TagId })
{
}

bool FAvaTagHandleContainer::ContainsTag(const FAvaTagHandle& InTagHandle) const
{
	if (!Source)
	{
		return false;
	}

	if (ContainsTagHandle(InTagHandle))
	{
		return true;
	}

	const FAvaTag* OtherTag = InTagHandle.GetTag();
	if (!OtherTag)
	{
		return false;
	}

	for (const FAvaTagId& TagId : TagIds)
	{
		const FAvaTag* Tag = Source->GetTag(TagId);
		if (Tag && *Tag == *OtherTag)
		{
			return true;
		}
	}

	return false;
}

bool FAvaTagHandleContainer::ContainsTagHandle(const FAvaTagHandle& InTagHandle) const
{
	return Source == InTagHandle.Source && TagIds.Contains(InTagHandle.TagId);
}

FString FAvaTagHandleContainer::ToString() const
{
	if (!Source)
	{
		return FString();
	}

	FString OutString;
	for (const FAvaTagId& TagId : TagIds)
	{
		if (const FAvaTag* Tag = Source->GetTag(TagId))
		{
			if (!OutString.IsEmpty())
			{
				OutString.Append(TEXT(", "));
			}
			OutString.Append(Tag->ToString());
		}
	}
	return OutString;
}

void FAvaTagHandleContainer::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		for (const FAvaTagId& TagId : TagIds)
		{
			if (TagId.IsValid())
			{
				Ar.MarkSearchableName(FAvaTagId::StaticStruct(), *TagId.ToString());
			}
		}
	}
}

bool FAvaTagHandleContainer::SerializeFromMismatchedTag(const FPropertyTag& InPropertyTag, FStructuredArchive::FSlot InSlot)
{
	static const FName TagHandleContextName = FAvaTagHandle::StaticStruct()->GetFName();

	if (InPropertyTag.GetType().IsStruct(TagHandleContextName))
	{
		FAvaTagHandle TagHandle;
		FAvaTagHandle::StaticStruct()->SerializeItem(InSlot, &TagHandle, nullptr);

		if (TagHandle.IsValid())
		{
			Source = TagHandle.Source;
			TagIds = { TagHandle.TagId };
		}
		return true;
	}

	return false;
}

bool FAvaTagHandleContainer::AddTagHandle(const FAvaTagHandle& InTagHandle)
{
	// Set Source to latest added tag handle
	if (!Source)
	{
		Source = InTagHandle.Source;
	}

	if (TagIds.Contains(InTagHandle.TagId))
	{
		return false;
	}

	TagIds.Add(InTagHandle.TagId);
	return true;
}

bool FAvaTagHandleContainer::RemoveTagHandle(const FAvaTagHandle& InTagHandle)
{
	return TagIds.Remove(InTagHandle.TagId) > 0;
}

TArray<FAvaTag> FAvaTagHandleContainer::ResolveTags() const
{
	TArray<FAvaTag> Tags;
	if (!Source)
	{
		return Tags;
	}

	Tags.Reserve(TagIds.Num());

	for (const FAvaTagId& TagId : TagIds)
	{
		if (const FAvaTag* Tag = Source->GetTag(TagId))
		{
			Tags.Add(*Tag);
		}
	}

	return Tags;
}
