// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementSlateWidgetColumns.generated.h"

class SWidget;

/**
 * Stores a widget reference in the data storage. At the start of processing any
 * columns that are not pointing to a valid widget will be removed. If the
 * FTypedElementSlateWidgetDeletesRowTag is found then the entire row will
 * be deleted.
 */
USTRUCT()
struct FTypedElementSlateWidgetReferenceColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	TWeakPtr<SWidget> Widget;
};

/**
 * Tag to indicate that the entire row needs to be deleted when the widget in
 * FTypedElementSlateWidgetReferenceColumn is no longer valid, otherwise only
 * the column will be removed.
 */
USTRUCT()
struct FTypedElementSlateWidgetReferenceDeletesRowTag : public FTypedElementDataStorageTag
{
	GENERATED_BODY()
};