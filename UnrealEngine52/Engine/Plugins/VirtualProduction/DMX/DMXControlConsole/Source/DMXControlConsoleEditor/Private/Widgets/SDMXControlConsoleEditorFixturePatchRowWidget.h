// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Library/DMXEntityReference.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FSlateBrush;


/** A Fixture Patch Row to show/select a Fixture Patch from a DMX Library */
class SDMXControlConsoleEditorFixturePatchRowWidget
	: public SCompoundWidget
{
	DECLARE_DELEGATE_OneParam(FDMXFixturePatchRowDelegate, const TSharedRef<SDMXControlConsoleEditorFixturePatchRowWidget>&)

public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFixturePatchRowWidget)
	{}
		/** Delegate excecuted when this widget's add next button is clicked */
		SLATE_EVENT(FDMXFixturePatchRowDelegate, OnGenerateOnLastRow)

		/** Delegate excecuted when this widget's add row button is clicked */
		SLATE_EVENT(FDMXFixturePatchRowDelegate, OnGenerateOnNewRow)

		/** Delegate excecuted when this widget's generate button is clicked */
		SLATE_EVENT(FDMXFixturePatchRowDelegate, OnGenerateOnSelectedFaderGroup)

		/** Delegate excecuted when this widget is selected */
		SLATE_EVENT(FDMXFixturePatchRowDelegate, OnSelectFixturePatchRow)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const FDMXEntityFixturePatchRef InFixturePatchRef);

	/** Selects this Details Row */
	void Select();

	/** Unselects this Details Row */
	void Unselect();

	/** True if this Fader Group is selected, otherwise false */
	bool IsSelected() const { return bSelected; }

	/** Gets Fixture Patch reference */
	const FDMXEntityFixturePatchRef& GetFixturePatchRef() const { return FixturePatchRef; }

protected:
	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End of SWidget interface

private:
	/** Called to generate a FaderGroup from Fixture Patch, on last row */
	FReply OnAddNextClicked();

	/** Called to generate a FaderGroup from Fixture Patch, on a new row */
	FReply OnAddRowClicked();

	/** Called to use a Fixture Patch to modify the selected Fader Group */
	FReply OnGenerateClicked();

	/** Gets border brush depending on selection state */
	const FSlateBrush* GetBorderImage() const;

	/** Gets add next button visibility depending on selection state */
	EVisibility GetAddNextButtonVisibility() const;

	/** Gets add row button visibility depending on selection state */
	EVisibility GetAddRowButtonVisibility() const;
	
	/** Gets generate button visibility depending on selection state */
	EVisibility GetGenerateButtonVisibility() const;

	/** The delegate to to excecute when this widget's add next button is clicked */
	FDMXFixturePatchRowDelegate OnGenerateOnLastRow;

	/** The delegate to to excecute when this widget's add row button is clicked */
	FDMXFixturePatchRowDelegate OnGenerateOnNewRow;

	/** The delegate to to excecute when this widget's generate button is clicked */
	FDMXFixturePatchRowDelegate OnGenerateOnSelectedFaderGroup;

	/** The delegate to to excecute when this widget is selected */
	FDMXFixturePatchRowDelegate OnSelectFixturePatchRow;

	/** Weak Object reference to the Fixture Patch this widget is based on */
	FDMXEntityFixturePatchRef FixturePatchRef;

	/** Shows wheter this Fader Group needs to be refreshed or not */
	bool bSelected = false;
};
