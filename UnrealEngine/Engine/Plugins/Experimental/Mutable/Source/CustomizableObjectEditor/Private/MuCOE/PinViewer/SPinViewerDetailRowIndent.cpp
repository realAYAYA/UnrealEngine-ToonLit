// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/PinViewer/SPinViewerDetailRowIndent.h"

#include "Layout/Children.h"
#include "Layout/Geometry.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "MuCOE/Widgets/SMutableExpandableTableRow.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateColor.h"
#include "Types/SlateStructs.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"

class FPaintArgs;
class FSlateRect;
class FWidgetStyle;
struct FSlateBrush;


void SPinViewerDetailRowIndent::Construct(const FArguments& InArgs, TSharedRef<SWidget> DetailsRow)
{
	Row = DetailsRow;
	
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(16.0f)
	];
}


int32 SPinViewerDetailRowIndent::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{

	const FSlateBrush* BackgroundBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
	const FSlateBrush* DropShadowBrush = FAppStyle::Get().GetBrush("DetailsView.ArrayDropShadow");

	const TSharedPtr<SWidget> RowPtr = Row.Pin();
	const FSlateColor BackgroundColor = GetRowBackgroundColor(0, RowPtr.IsValid() && RowPtr->IsHovered());
	
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2D(0, 0), FVector2D(16, AllottedGeometry.GetLocalSize().Y)),
		BackgroundBrush,
		ESlateDrawEffect::None,
		BackgroundColor.GetColor(InWidgetStyle)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(FVector2D(0, 0), FVector2D(16, AllottedGeometry.GetLocalSize().Y)),
		DropShadowBrush
	);

	return LayerId + 1;
}

