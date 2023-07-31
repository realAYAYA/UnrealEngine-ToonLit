// Copyright Epic Games, Inc. All Rights Reserved.

#include "Layout/ArrangedChildren.h"
#include "Widgets/SWidget.h"


/* FArrangedChildren interface
 *****************************************************************************/

void FArrangedChildren::AddWidget(const FArrangedWidget& InWidgetGeometry)
{
	AddWidget(InWidgetGeometry.Widget->GetVisibility(), InWidgetGeometry);
}

void FArrangedChildren::InsertWidget(const FArrangedWidget& InWidgetGeometry, int32 Index)
{
	InsertWidget(InWidgetGeometry.Widget->GetVisibility(), InWidgetGeometry, Index);
}