// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/CurveEditorFilterBase.h"
#include "CurveEditor.h"


bool UCurveEditorFilterBase::CanApplyFilter_Impl(TSharedRef<FCurveEditor> InCurveEditor)
{
	return InCurveEditor->GetSelection().Count() > 0;
}
