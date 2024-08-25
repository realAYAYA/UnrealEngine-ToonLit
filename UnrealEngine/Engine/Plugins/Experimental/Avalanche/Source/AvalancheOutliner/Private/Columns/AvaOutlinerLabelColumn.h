// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Columns/IAvaOutlinerColumn.h"

class FAvaOutlinerLabelColumn : public IAvaOutlinerColumn
{
public:
	UE_AVA_INHERITS(FAvaOutlinerLabelColumn, IAvaOutlinerColumn);
	
protected:
	//~ Begin IAvaOutlinerColumn
	virtual FText GetColumnDisplayNameText() const override;
	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	virtual TSharedRef<SWidget> ConstructRowWidget(FAvaOutlinerItemPtr InItem
		, const TSharedRef<FAvaOutlinerView>& InOutlinerView
		, const TSharedRef<SAvaOutlinerTreeRow>& InRow) override;
	//~ End IAvaOutlinerColumn
};
