// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"

class FMaterialEditor;

class SMaterialEditorStrataWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialEditorStrataWidget)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FMaterialEditor> InMaterialEditorPtr);

	/** Gets the widget contents of the app */
	virtual TSharedRef<SWidget> GetContent();

	virtual ~SMaterialEditorStrataWidget();

	/** SWidget interface */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	TSharedPtr<class SCheckBox> CheckBoxForceFullSimplification;

	TSharedPtr<class SButton> ButtonApplyToPreview;

	FReply OnButtonApplyToPreview();

	/** Pointer back to the material editor that owns this */
	TWeakPtr<FMaterialEditor> MaterialEditorPtr;
};

