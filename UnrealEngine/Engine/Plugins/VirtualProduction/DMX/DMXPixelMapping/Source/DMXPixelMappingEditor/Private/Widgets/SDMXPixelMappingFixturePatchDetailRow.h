// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UDMXEntityFixturePatch;

class IPropertyHandle;
class STextBlock;


/** A fixture patch in a detail row, supposedly shown as an array of patches */
class SDMXPixelMappingFixturePatchDetailRow
	: public SCompoundWidget
{
	DECLARE_DELEGATE_TwoParams(FDMXOnFixturePatchDetailRowMouseButtonEvent, const FGeometry&, const FPointerEvent&);
	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FDMXOnFixturePatchDetailRowDragged, const FGeometry&, const FPointerEvent&);

public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingFixturePatchDetailRow) {}

		SLATE_ARGUMENT(TWeakObjectPtr<UDMXEntityFixturePatch>, FixturePatch)

		SLATE_EVENT(FDMXOnFixturePatchDetailRowMouseButtonEvent, OnLMBDown)

		SLATE_EVENT(FDMXOnFixturePatchDetailRowMouseButtonEvent, OnLMBUp)

		SLATE_EVENT(FDMXOnFixturePatchDetailRowDragged, OnDragged)
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Sets a highlight color to indicate selection */
	void SetHighlight(bool bNewHighlight) { bHighlight = bNewHighlight; }

	/** Sets an error Text. If the text is empty, no error shows. */
	void SetErrorText(FText InErrorText) { ErrorText = InErrorText; }

protected:
	// ~Begin SWidget Interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// ~End SWidget Interface

private:
	/** Text that displays the fixture patch name */
	TSharedPtr<STextBlock> FixturePatchNameTextBlock;

	/** If true is highlit */
	bool bHighlight = false;

	/** Error text, shows if not empty */
	FText ErrorText;
	
	// Slate Args
	FDMXOnFixturePatchDetailRowMouseButtonEvent OnLMBDown;
	FDMXOnFixturePatchDetailRowMouseButtonEvent OnLMBUp;
	FDMXOnFixturePatchDetailRowDragged OnDragged;
};
