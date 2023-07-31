// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "SNPWindow.h"

class SNPWindow;

class SNPSimFrameContents : public SCompoundWidget
{
public:

	SNPSimFrameContents();
	virtual ~SNPSimFrameContents();

	void Reset();

	SLATE_BEGIN_ARGS(SNPSimFrameContents) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SNPWindow> InNPWindow);

	void NotifyContentClicked(const FSimContentsView& InView);

	TSharedPtr<SHorizontalBox> ContentsHBoxPtr;
	TSharedPtr<SVerticalBox> SystemFaultsVBoxPtr;

	TSharedPtr<STextBlock> SimInfoTextBlock;
	TSharedPtr<STextBlock> SimTickTextBlock;

	TSharedPtr<STextBlock> InputCmdTextBlock;
	TSharedPtr<STextBlock> SyncStateTextBlock;
	TSharedPtr<STextBlock> AuxStateTextBlock;

	TSharedPtr<SNPWindow> NPWindow;
};