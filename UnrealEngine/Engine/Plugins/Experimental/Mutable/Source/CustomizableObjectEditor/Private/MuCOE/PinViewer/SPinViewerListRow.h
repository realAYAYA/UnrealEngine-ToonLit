// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphPin.h"
#include "MuCOE/Widgets/SMutableExpandableTableRow.h"
#include "Widgets/Views/ITableRow.h"

class SPinViewer;
class STableViewBase;
class SWidget;

/** Custom row for a SListView that allows to add custom widgets as a row element. */
class SPinViewerListRow : public SMutableExpandableTableRow<TSharedPtr<FEdGraphPinReference>>
{
public:
	SLATE_BEGIN_ARGS(SPinViewerListRow) {}
	SLATE_ARGUMENT(SPinViewer*, PinViewer)
	SLATE_ARGUMENT(FEdGraphPinReference, PinReference)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView);

	// SPinViewerMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	virtual TSharedPtr<SWidget> GenerateAdditionalWidgetForRow() override;
	virtual EVisibility GetAdditionalWidgetDefaultVisibility() const override;
	virtual void SetAdditionalWidgetVisibility(EVisibility Visibility) override;
	
	// Own interface
	/** CheckBox callback. */
	void OnPinVisibilityCheckStateChanged(ECheckBoxState NewRadioState);
	
	/** Returns the state of the check box in function of the visibility of the pin */
	ECheckBoxState IsVisibilityChecked() const;

private:
	SPinViewer* PinViewer = nullptr;
	
	/** Pin representing this row. */
	FEdGraphPinReference PinReference;
	FGuid PinId; // Temporal performance fix. Avoids using the slow FEdPinReference hash function.
};
