// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementValueCacheColumns.generated.h"


/**
 * Column that can be used to cache an unsigned 32-bit value in.
 */
USTRUCT()
struct FTypedElementU32IntValueCacheColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	uint32 Value;
};

/**
 * Column that can be used to cache a signed 32-bit value in.
 */
USTRUCT()
struct FTypedElementI32IntValueCacheColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	int32 Value;
};

/**
 * Column that can be used to cache a 32-bit floating point value in.
 */
USTRUCT()
struct FTypedElementFloatValueCacheColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	float Value;
};