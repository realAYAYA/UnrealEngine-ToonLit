// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SEditorViewportViewMenu.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Viewport View modes menu used by Chaos Visual Debugger.
 * Compared to the normal Editor View modes menu, it has a reduced set of modes available,
 * showing only the ones supported
 */
class SChaosVDEditorViewportViewMenu : public SEditorViewportViewMenu
{
public:
	SLATE_BEGIN_ARGS(SChaosVDEditorViewportViewMenu){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<class SViewportToolBar> InParentToolBar);

protected:
	virtual void RegisterMenus() const override;
};
