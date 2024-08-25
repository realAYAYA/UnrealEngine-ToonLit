// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTag.h"
#include "Containers/Set.h"
#include "UObject/Object.h"
#include "AvaTagCollection.generated.h"

/**
 * Tag Collection that identifies a tag with an underlying Tag Id Guid
 * and provides Tag reference capabilities
 */
UCLASS()
class AVALANCHETAG_API UAvaTagCollection : public UObject
{
	GENERATED_BODY()

public:
	const FAvaTag* GetTag(const FAvaTagId& InTagId) const;

	/** Returns the keys of the Tag Map */
	TArray<FAvaTagId> GetTagIds() const;

	static FName GetTagMapName();

private:
	UPROPERTY(EditAnywhere, Category="Tag")
	TMap<FAvaTagId, FAvaTag> Tags;
};
