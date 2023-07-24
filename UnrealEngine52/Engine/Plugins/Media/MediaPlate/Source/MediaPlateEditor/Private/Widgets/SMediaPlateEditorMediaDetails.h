// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class UMediaPlateComponent;

/**
 * Implements the details panel of the MediaPlate asset editor.
 */
class SMediaPlateEditorMediaDetails
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlateEditorMediaDetails) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs		The declaration data for this widget.
	 * @param InMediaPlate	The MediaPlate to show the details for.
	 */
	void Construct(const FArguments& InArgs, UMediaPlateComponent& InMediaPlate);

	//~ SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	/**
	 * Updates our widgets to reflect the current state.
	 */
	void UpdateDetails();

	/** Pointer to the MediaPlate that is being viewed. */
	TWeakObjectPtr<UMediaPlateComponent> MediaPlate;

	/** Our widgets. */
	TSharedPtr<STextBlock> FormatText;
	TSharedPtr<STextBlock> FrameRateText;
	TSharedPtr<STextBlock> LODBiasText;
	TSharedPtr<STextBlock> MethodText;
	TSharedPtr<STextBlock> NumMipsText;
	TSharedPtr<STextBlock> NumTilesText;
	TSharedPtr<STextBlock> ResolutionText;
	TSharedPtr<STextBlock> ResourceSizeText;
};
