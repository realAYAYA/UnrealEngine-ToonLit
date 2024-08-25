// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementCounterWidget.generated.h"

class SWindow;

UCLASS()
class TYPEDELEMENTSDATASTORAGEUI_API UTypedElementCounterWidgetFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	static FName WigetPurpose;

	UTypedElementCounterWidgetFactory();
	~UTypedElementCounterWidgetFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;
	void RegisterWidgetPurposes(ITypedElementDataStorageUiInterface& DataStorageUi) const override;
	void RegisterWidgetConstructors(ITypedElementDataStorageInterface& DataStorage,
		ITypedElementDataStorageUiInterface& DataStorageUi) const override;

	static void EnableCounterWidgets();

private:
	static void SetupMainWindowIntegrations(TSharedPtr<SWindow> ParentWindow, bool bIsRunningStartupDialog);

	static bool bAreCounterWidgetsEnabled;
	static bool bHasBeenSetup;
};

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
	FTypedElementCounterWidgetConstructor();
	~FTypedElementCounterWidgetConstructor() override = default;

	TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;

	FText ToolTipText{ NSLOCTEXT("TypedElementUI_CounterWidget", "Tooltip", "Shows the total number found in the editor.") };
	FText LabelText{ NSLOCTEXT("TypedElementUI_CounterWidget", "Label", "Counted") };
	TypedElementQueryHandle Query;

protected:
	TSharedPtr<SWidget> CreateWidget(const TypedElementDataStorage::FMetaDataView& Arguments) override;
	bool SetColumns(ITypedElementDataStorageInterface* DataStorage, TypedElementRowHandle Row) override;
};

USTRUCT(meta = (DisplayName = "Counter widget"))
struct TYPEDELEMENTSDATASTORAGEUI_API FTypedElementCounterWidgetColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	FTextFormat LabelTextFormatter;
	TypedElementQueryHandle Query;
};