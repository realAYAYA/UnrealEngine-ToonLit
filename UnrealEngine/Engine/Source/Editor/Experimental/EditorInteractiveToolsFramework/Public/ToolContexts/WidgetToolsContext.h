// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdModeInteractiveToolsContext.h"
#include "HAL/Platform.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "WidgetToolsContext.generated.h"

class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
class UObject;
struct FCaptureLostEvent;
struct FCharacterEvent;
struct FGeometry;
struct FKeyEvent;
struct FPointerEvent;

/**
 * UWidgetToolsContext extends UModeManagerInteractiveToolsContext with methods needed for
 * tools operating on general widgets that do not have a viewport.
 */
UCLASS(Transient)
class EDITORINTERACTIVETOOLSFRAMEWORK_API UWidgetToolsContext : public UModeManagerInteractiveToolsContext
{
	GENERATED_BODY()

public:

	// ~Begin SWidget "Proxy" Interface
	bool OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent);
	bool OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	bool OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	bool OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	bool OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	bool OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	bool OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	bool OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);
	bool OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent);
	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;
	// ~End SWidget "Proxy" Interface
};