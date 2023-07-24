// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementCounterWidgetConstructor.generated.h"

/**
 * Constructor for the counter widget. The counter widget accepts a "count"-query. The query will be periodically
 * run and the result is written to a textbox widget after it's been formatted using LabelText. An example for 
 * LabelText is "{0} {0}|plural(one=MyCounter, other=MyCounters)" which will use "MyCounter" if there's exactly one
 * entry found and otherwise "MyCounters".
 */
USTRUCT()
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementCounterWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	~FTypedElementCounterWidgetConstructor() override = default;

	FText ToolTipText{ NSLOCTEXT("TypedElementUI_CounterWidget", "Tooltip", "Shows the total number found in the editor.") };
	FText LabelText{ NSLOCTEXT("TypedElementUI_CounterWidget", "Label", "Counted") };
	TypedElementQueryHandle Query;

	static void Register(ITypedElementDataStorageInterface& DataStorage, ITypedElementDataStorageUiInterface& DataStorageUi);

protected:
	TSharedPtr<SWidget> CreateWidget() override;
	void AddColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT()
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementCounterWidgetColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	FTextFormat LabelTextFormatter;
	TypedElementQueryHandle Query;
};