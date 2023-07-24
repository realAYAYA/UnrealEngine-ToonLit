// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
//#include "SEditorViewport.h"
#include "Widgets/SGroomEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

//class FMenuBuilder;

/**
 * A level viewport toolbar widget that is placed in a viewport
 */
class SGroomEditorViewportToolbar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SGroomEditorViewportToolbar) {}
	SLATE_END_ARGS()

	/**
	 * Constructs the toolbar
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider);

private:

	/**
	* Generates the show menu content
	*/
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;

	/**
	* Adds the LOD menu to the left-aligned toolbar slots
	*/
	virtual void ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const override;

	/**
	* Generates the toolbar LOD menu content
	*/
	TSharedRef<SWidget> GenerateLODMenu() const;

	/**
	* Returns the label for the "LOD" tool bar menu, which changes depending on the current LOD selection
	*
	* @return	Label to use for this LOD label
	*/
	FText GetLODMenuLabel() const;
};

