// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

class FCurveEditor;
class FMenuBuilder;
struct FCurveModelID;
struct FCurvePointHandle;

struct FCurveEditorContextMenu
{
	static void BuildMenu(FMenuBuilder& MenuBuilder, TSharedRef<FCurveEditor> CurveEditor, TOptional<FCurvePointHandle> ClickedPoint, TOptional<FCurveModelID> HoveredCurve);
};