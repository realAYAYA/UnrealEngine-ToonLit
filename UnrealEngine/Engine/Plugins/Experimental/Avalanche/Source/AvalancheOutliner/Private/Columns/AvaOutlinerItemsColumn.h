// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Columns/IAvaOutlinerColumn.h"

/**
 * Column to help visualize Child Items of a given Item when the Item is collapsed
 */
class FAvaOutlinerItemsColumn : public IAvaOutlinerColumn
{
public:
	UE_AVA_INHERITS(FAvaOutlinerItemsColumn, IAvaOutlinerColumn);
	
protected:
	//~ Begin IAvaOutlinerColumn
	virtual FText GetColumnDisplayNameText() const override;
	virtual bool ShouldShowColumnByDefault() const override { return true; }
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	virtual TSharedRef<SWidget> ConstructRowWidget(FAvaOutlinerItemPtr InItem
		, const TSharedRef<FAvaOutlinerView>& InOutlinerView
		, const TSharedRef<SAvaOutlinerTreeRow>& InRow) override;
	//~ End IAvaOutlinerColumn
};
