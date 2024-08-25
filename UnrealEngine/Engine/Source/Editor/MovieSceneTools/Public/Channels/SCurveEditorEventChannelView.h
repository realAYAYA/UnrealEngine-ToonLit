// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Math/Vector2D.h"
#include "Rendering/RenderingCommon.h"
#include "Templates/SharedPointer.h"
#include "Views/SInteractiveCurveEditorView.h"

class FCurveEditor;
class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FText;
class FWidgetStyle;
struct FGeometry;

class MOVIESCENETOOLS_API SCurveEditorEventChannelView : public SInteractiveCurveEditorView
{
public:

	void Construct(const FArguments& InArgs, TWeakPtr<FCurveEditor> InCurveEditor);

	virtual void GetGridLinesY(TSharedRef<const FCurveEditor> CurveEditor, TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels) const override;

private:

	//~ SWidget Interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	//~ SInteractiveCurveEditorView interface
	virtual void PaintView(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool IsValueSnapEnabled() const override { return false; }

	void DrawLabels(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const;

	static float TrackHeight;
};
