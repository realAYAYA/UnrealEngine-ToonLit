// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Options for adding a new ref
 */ 
struct HORDE_API FRefOptions
{
	static const FRefOptions Default;

	/** Time until a ref is expired. If zero, the ref does not expire. */
	FTimespan Lifetime;

	/** Whether to extend the remaining lifetime of a ref whenever it is fetched. Defaults to true. */
	bool bExtend = true;

	FRefOptions();
};
