// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Math/MathFwd.h"
#include "Misc/Attribute.h"
#include "MuR/Layout.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SCustomizableObjectLayoutGrid;
struct FGeometry;


/** Widget showing a Mutable layout, with its properties. */
class SMutableLayoutViewer : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS(SMutableLayoutViewer) {}
		SLATE_ATTRIBUTE(FIntPoint, GridSize)
		SLATE_ARGUMENT(mu::LayoutPtrConst, Layout)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// Own interface

	/** Set the image to show in the widget. */
	void SetLayout(const mu::LayoutPtrConst& Image );

private:

	/** Brush used to render the layout. */
	TSharedPtr<SCustomizableObjectLayoutGrid> LayoutViewer;

	/** Mutable Layout object being previewed */
	mu::LayoutPtrConst MutableLayout;

	/** Is true, the image or the visible LOD have changed and we need to update. */
	bool bIsPendingUpdate = false;

	/** User interface callbacks */
	FText GetLayoutDescriptionLabel() const;
};
