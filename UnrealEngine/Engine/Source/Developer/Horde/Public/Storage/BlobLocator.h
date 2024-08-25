// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Utf8String.h"

/**
 * Identifies the location of a blob. Meaning of this string is implementation defined.
 */
class HORDE_API FBlobLocator
{
public:
	FBlobLocator();
	FBlobLocator(FUtf8String InPath);
	FBlobLocator(const FBlobLocator& InBaseLocator, const FUtf8StringView& InFragment);
	explicit FBlobLocator(const FUtf8StringView& InPath);

	/** Tests whether this locator is valid. */
	bool IsValid() const;

	/** Gets the path for this locator. */
	const FUtf8String& GetPath() const;

	/** Gets the base locator for this blob. */
	FBlobLocator GetBaseLocator() const;

	/** Gets the path for this locator. */
	FUtf8StringView GetFragment() const;

	/** Determines if this locator can be unwrapped into an outer locator/fragment pair. */
	bool CanUnwrap() const;

	/** Split this locator into a locator and fragment. */
	bool TryUnwrap(FBlobLocator& OutLocator, FUtf8StringView& OutFragment) const;

	bool operator==(const FBlobLocator& Other) const;
	bool operator!=(const FBlobLocator& Other) const;

	friend uint32 GetTypeHash(const FBlobLocator& Locator);

private:
	FUtf8String Path;
};