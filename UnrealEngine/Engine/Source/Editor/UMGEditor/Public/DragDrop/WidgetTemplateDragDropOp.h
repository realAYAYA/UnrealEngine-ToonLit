// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Input/DragAndDrop.h"
#include "Templates/SharedPointer.h"

class FWidgetTemplate;

/**
 * This drag drop operation allows widget templates from the palate to be dragged and dropped into the designer
 * or the widget hierarchy in order to spawn new widgets.
 */
class UMGEDITOR_API FWidgetTemplateDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FWidgetTemplateDragDropOp, FDecoratedDragDropOp)

	/** The template to create an instance */
	TSharedPtr<FWidgetTemplate> Template;

	/** Constructs the drag drop operation */
	static TSharedRef<FWidgetTemplateDragDropOp> New(const TSharedPtr<FWidgetTemplate>& InTemplate);
};
