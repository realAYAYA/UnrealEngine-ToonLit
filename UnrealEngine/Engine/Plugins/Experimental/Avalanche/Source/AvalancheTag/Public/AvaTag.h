// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Misc/Guid.h"
#include "UObject/NameTypes.h"
#include "AvaTag.generated.h"

/**
 * Struct for a Tag
 * NOTE: Do not use as storage type. Prefer using FAvaTagHandle to then retrieve the FAvaTag it references
 */
USTRUCT(BlueprintType)
struct FAvaTag
{
	GENERATED_BODY()

	bool IsValid() const
	{
		return TagName != NAME_None;
	}

	bool operator==(const FAvaTag& InTag) const
	{
		return TagName == InTag.TagName;
	}

	friend uint32 GetTypeHash(const FAvaTag& InTag)
	{
		return GetTypeHash(InTag.TagName);
	}

	FString ToString() const
	{
		return TagName.ToString();
	}

	UPROPERTY(EditAnywhere, Category="Tag")
	FName TagName;
};

/**
 * Struct to identify a Tag. This is used by FAvaTagHandle to reference an FAvaTag
 */
USTRUCT()
struct FAvaTagId
{
	GENERATED_BODY()

	/** Id is initialized to a zero */
	FAvaTagId() = default;

	/**
	 * Force init where Id is initialized to a new guid.
	 * This is used in TCppStructOps::Construct
	 */
	explicit FAvaTagId(EForceInit)
		: Id(FGuid::NewGuid())
	{
	}

	explicit FAvaTagId(const FGuid& InId)
		: Id(InId)
	{
	}

	bool operator==(const FAvaTagId& InOther) const
	{
		return Id == InOther.Id;
	}

	bool IsValid() const
	{
		return Id.IsValid();
	}

	FString ToString() const
	{
		return Id.ToString();
	}

	friend uint32 GetTypeHash(const FAvaTagId& InTagId)
	{
		return GetTypeHash(InTagId.Id);
	}

private:
	UPROPERTY(EditAnywhere, Category="Tag", meta=(IgnoreForMemberInitializationTest))
	FGuid Id;
};

template<>
struct TStructOpsTypeTraits<FAvaTagId> : TStructOpsTypeTraitsBase2<FAvaTagId>
{
	enum
	{
		WithNoInitConstructor = true,
	};
};
