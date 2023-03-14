// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Layout/SBox.h"

/**
 * Wrapper widget meant to handle native-side painting for UCommonVisualAttachment.
 */
class COMMONUI_API SVisualAttachmentBox : public SBox
{
public:
	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	void SetContentAnchor(FVector2D InContentAnchor);

private:
	mutable FVector2D InnerDesiredSize;

	FVector2D ContentAnchor = FVector2D::ZeroVector;
};