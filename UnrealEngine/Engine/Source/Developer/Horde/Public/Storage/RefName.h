// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Utf8String.h"

// Identifier for a ref in the storage system. Refs serve as GC roots, and are persistent entry points to expanding data structures within the store.
struct HORDE_API FRefName
{
public:
	FRefName(FUtf8String Text);
	~FRefName();

	/** Accessor for the underlying string. */
	const FUtf8String& GetText() const;

	bool operator==(const FRefName& Other) const;
	bool operator!=(const FRefName& Other) const;
	friend uint32 GetTypeHash(const FRefName& RefName);

private:
	FUtf8String Text;
};
