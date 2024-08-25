// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAvaOutlinerLabelItem.h"

class FAvaOutlinerComponent;

class SAvaOutlinerLabelComponent : public SAvaOutlinerLabelItem
{
public:
	SLATE_BEGIN_ARGS(SAvaOutlinerLabelComponent) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
		, const TSharedRef<FAvaOutlinerComponent>& InItem
		, const TSharedRef<SAvaOutlinerTreeRow>& InRow);

	virtual const FInlineEditableTextBlockStyle* GetTextBlockStyle() const override;
};
