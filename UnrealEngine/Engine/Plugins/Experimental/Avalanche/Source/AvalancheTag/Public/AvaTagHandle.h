// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTag.h"
#include "Containers/ContainersFwd.h"
#include "AvaTagHandle.generated.h"

class UAvaTagCollection;

/**
 * Handle to a Tag in a particular Source.
 * This should be used by the UStructs/UObjects to properly reference a particular FAvaTag.
 */
USTRUCT()
struct AVALANCHETAG_API FAvaTagHandle
{
	GENERATED_BODY()

	FAvaTagHandle() = default;

	FAvaTagHandle(const UAvaTagCollection* InSource, const FAvaTagId& InTagId)
		: Source(InSource)
		, TagId(InTagId)
	{
	}

	const FAvaTag* GetTag() const;

	FString ToString() const;

	FString ToDebugString() const;

	FName ToName() const;

	void PostSerialize(const FArchive& Ar);

	/** Returns true if the Tag Handles resolve to same valued FAvaTags, even if the Source or Tag Id is different */
	bool MatchesTag(const FAvaTagHandle& InOther) const;

	/** Returns true if the Tag Handles is the exact same as the other (Same Source and Tag Id) */
	bool MatchesExact(const FAvaTagHandle& InOther) const;

	bool IsValid() const
	{
		return Source && TagId.IsValid();
	}

	friend uint32 GetTypeHash(const FAvaTagHandle& InHandle)
	{
		return HashCombineFast(GetTypeHash(InHandle.Source), GetTypeHash(InHandle.TagId));
	}

	UPROPERTY(EditAnywhere, Category="Tag")
	TObjectPtr<const UAvaTagCollection> Source;

	UPROPERTY()
	FAvaTagId TagId;
};

template<>
struct TStructOpsTypeTraits<FAvaTagHandle> : public TStructOpsTypeTraitsBase2<FAvaTagHandle>
{
	enum
	{
		WithPostSerialize = true,
	};
};
