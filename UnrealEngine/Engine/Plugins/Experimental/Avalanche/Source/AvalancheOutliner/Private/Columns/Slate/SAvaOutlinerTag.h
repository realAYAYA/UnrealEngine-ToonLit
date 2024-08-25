// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerDefines.h"
#include "Widgets/SCompoundWidget.h"

class SAvaOutlinerTreeRow;

class SAvaOutlinerTag : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaOutlinerTag) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const FAvaOutlinerItemPtr& InItem
		, const TSharedRef<SAvaOutlinerTreeRow>& InRow);

	FText GetText() const;

private:
	TWeakPtr<IAvaOutlinerItem> Item;
};
