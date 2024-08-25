// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"

#include "TypedElementMementoInterface.generated.h"

/**
 * Add to a row to opt into behaviour to populate a memento
 * when the row is deleted
 */
USTRUCT()
struct FTypedElementMementoOnDelete : public FTypedElementDataStorageColumn
{
	// The memento row populated when the row owning this column is deleted
	GENERATED_BODY()
	TypedElementRowHandle Memento;
};

/**
 * When a memento is populated, this column will be present
 */
USTRUCT()
struct FTypedElementMementoPopulated : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
};