// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Identifies the type of a blob
 */
struct HORDE_API FBlobType
{
	static const FBlobType Leaf;

	/** Nominal identifier for the type. */
	FGuid Guid;

	/** Version number for the serializer. */
	int Version;

	FBlobType(const FGuid& InGuid, int InVersion);

	bool operator==(const FBlobType& Other) const;
	bool operator!=(const FBlobType& Other) const;
};
