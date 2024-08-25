// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementAlertColumns.generated.h"

UENUM()
enum class FTypedElementAlertColumnType : uint8
{
	Error,
	Warning,

	MAX
};

/**
 * Column containing information a user needs to be alerted of.
 */
USTRUCT(meta = (DisplayName = "Alert"))
struct FTypedElementAlertColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY()
	FText Message;

	// Store a copy of the parent row so it's possible to detect if a row has been reparented.
	TypedElementDataStorage::RowHandle CachedParent;

	// The cycle id is 64 bits, but this column is only interested in avoiding the same update happening multiple times in the same frame
	// therefore this is kept small to stay within the padding of the struct. This could even be reduced to a 8 bit value if needed.
	uint16 RemoveCycleId;

	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FTypedElementAlertColumnType AlertType;
};

/**
 * Column containing a count for the number of alerts any child rows have.
 */
USTRUCT(meta = (DisplayName = "Child alert"))
struct FTypedElementChildAlertColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	// Store a copy of the parent row so it's possible to detect if a row has been reparented.
	TypedElementDataStorage::RowHandle CachedParent;

	uint16 Counts[static_cast<size_t>(FTypedElementAlertColumnType::MAX)];

	// The cycle id is 64 bits, but this column is only interested in avoiding the same update happening multiple times in the same frame
	// therefore this is kept small to stay within the padding of the struct. This could even be reduced to a 8 bit value if needed.
	uint16 RemoveCycleId;

	// Indicates if during updating recently, this column has already decremented its parents. This is only needed to avoid repeated
	// decrements if updating the child alerts takes more than a single frame. It's not used outside updating.
	bool bHasDecremented;
};
