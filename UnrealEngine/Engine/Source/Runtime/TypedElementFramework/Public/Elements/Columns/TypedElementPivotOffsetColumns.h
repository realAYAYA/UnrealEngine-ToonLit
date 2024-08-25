// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"

#include "TypedElementPivotOffsetColumns.generated.h"

USTRUCT()
struct FTypedElementPivotOffset final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (IgnoreForMemberInitializationTest))
	FVector Offset;
};