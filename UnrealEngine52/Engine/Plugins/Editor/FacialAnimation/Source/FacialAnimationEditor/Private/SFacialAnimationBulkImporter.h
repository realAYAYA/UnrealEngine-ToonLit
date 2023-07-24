// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class SFacialAnimationBulkImporter : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFacialAnimationBulkImporter)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	bool IsImportButtonEnabled() const;

	FReply HandleImportClicked();
};
