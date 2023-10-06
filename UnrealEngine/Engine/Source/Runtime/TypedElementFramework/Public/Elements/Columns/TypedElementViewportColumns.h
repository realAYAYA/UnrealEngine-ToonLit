// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementViewportColumns.generated.h"

/**
 * Column to hold the color that the object is outlined with when selected in the viewport
 */
USTRUCT(meta = (DisplayName = "Viewport Color"))
struct FTypedElementViewportColorColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY(meta = (ClampMin = "0", ClampMax = "7"))
	uint8 SelectionOutlineColorIndex = 0;
};
