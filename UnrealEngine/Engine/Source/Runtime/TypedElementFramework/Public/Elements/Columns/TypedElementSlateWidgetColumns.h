// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementSlateWidgetColumns.generated.h"

class SWidget;
class STedsWidget;

/**
 * Stores a widget reference in the data storage. At the start of processing any
 * columns that are not pointing to a valid widget will be removed. If the
 * FTypedElementSlateWidgetDeletesRowTag is found then the entire row will
 * be deleted.
 */
USTRUCT(meta = (DisplayName = "Slate widget reference"))
struct FTypedElementSlateWidgetReferenceColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	// The actual internal widget
	TWeakPtr<SWidget> Widget;

	// Reference to the container widget that holds the internal widget
	TWeakPtr<STedsWidget> TedsWidget;
};

/**
 * Tag to indicate that the entire row needs to be deleted when the widget in
 * FTypedElementSlateWidgetReferenceColumn is no longer valid, otherwise only
 * the column will be removed.
 */
USTRUCT(meta = (DisplayName = "Slate widget reference deletes row"))
struct FTypedElementSlateWidgetReferenceDeletesRowTag final : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};