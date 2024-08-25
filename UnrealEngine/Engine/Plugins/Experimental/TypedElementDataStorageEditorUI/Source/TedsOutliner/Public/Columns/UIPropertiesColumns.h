// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Widgets/Views/SHeaderRow.h"

#include "UIPropertiesColumns.generated.h"

USTRUCT()
struct FUIHeaderPropertiesColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	EColumnSizeMode::Type ColumnSizeMode;

	/*
	 * Fill: Column stretches to this fraction of the header row
	 * Fixed: Column is fixed at this width in slate units and cannot be resized
	 * Manual: Column defaults to this width in slate units and can be user-sized
	 * FillSized: Column stretches as Fill but is initialized with this width in slate units
	 */
	UPROPERTY()
	float Width;
};
