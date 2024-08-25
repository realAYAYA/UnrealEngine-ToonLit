// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementHiearchyColumns.generated.h"

/**
 * A reference to the direct hierarchical parent of this row.
 */
USTRUCT(meta = (DisplayName = "Parent"))
struct FTypedElementParentColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	TypedElementDataStorage::RowHandle Parent;
};

/**
 * A reference to the direct hierarchical parent of this row which has not been resolved yet. The stored value will
 * be used to attempt to find the indexed row. This column can not be used to find rows that are not indexed.
 */
USTRUCT(meta = (DisplayName = "Parent (Unresolved)"))
struct FTypedElementUnresolvedParentColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	uint64 ParentIdHash;
};
