// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class UGSTab;

class SEmptyTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEmptyTab) {}
		SLATE_ARGUMENT(UGSTab*, Tab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnOpenProjectClicked();

	UGSTab* Tab;
};
