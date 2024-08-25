// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FAvaLevelViewportClient;

class SAvaLevelViewportSafeFrames : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaLevelViewportSafeFrames)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FAvaLevelViewportClient> InAvaLevelViewportClient);

	//~ Begin SWidget
	virtual int32 OnPaint(const FPaintArgs& InPaintArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InMyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool bInParentEnabled) const override;
	//~ End SWidget

protected:
	TWeakPtr<FAvaLevelViewportClient> AvaLevelViewportClientWeak;

	void DrawSafeFrame(const FPaintArgs& InPaintArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InMyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32& InOutLayerId, float InScreenPercentage, const FLinearColor& InLineColor, 
		const float InLineThickness) const;
};
