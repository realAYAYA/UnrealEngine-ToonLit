// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Memento/TypedElementMementoTranslators.h"
#include "TypedElementMementoizedColumns.generated.h"

/**
 * Enable SelectionColumn to be mementoized
 */
UCLASS()
class UTypedElementSelectionColumnMementoTranslator final : public UTypedElementDefaultMementoTranslator
{
	GENERATED_BODY()
public:
	virtual const UScriptStruct* GetColumnType() const override { return FTypedElementSelectionColumn::StaticStruct(); }
};