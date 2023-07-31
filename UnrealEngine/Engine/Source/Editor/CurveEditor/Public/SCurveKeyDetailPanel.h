// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveEditor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCurveEditor;
// Forward Declare
class IPropertyRowGenerator;
class SWidget;

/**
 * Inline details panel that lets you edit the Time and Value of a generic FCurveEditor Key
 */
class CURVEEDITOR_API SCurveKeyDetailPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCurveKeyDetailPanel)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FCurveEditor> InCurveEditor);
	TSharedPtr<IPropertyRowGenerator> GetPropertyRowGenerator() const { return PropertyRowGenerator; }

private:
	void PropertyRowsRefreshed();
	void ConstructChildLayout(TSharedPtr<SWidget> TimeWidget, TSharedPtr<SWidget> ValueWidget);

private:
	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;

	TSharedPtr<SWidget> TempTimeWidget;
	TSharedPtr<SWidget> TempValueWidget;
};