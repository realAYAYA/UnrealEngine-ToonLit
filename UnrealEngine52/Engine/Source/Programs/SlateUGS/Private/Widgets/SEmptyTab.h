// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class UGSTab;

class SEmptyTab final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEmptyTab) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UGSTab* InTab);

private:
	FReply OnOpenProjectClicked();

	UGSTab* Tab = nullptr;
};
