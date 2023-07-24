// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISkeletalMeshEditor.h"

#include "CoreMinimal.h"
#include "StatusBarSubsystem.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SCompoundWidget.h"


class FModeToolkit;
class ISkeletalMeshEditor;
class IToolkit;
class SBorder;


class SSkeletalMeshEditorToolbox :
	public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSkeletalMeshEditorToolbox ) {}
	SLATE_END_ARGS()

	~SSkeletalMeshEditorToolbox();

	void Construct(const FArguments& InArgs, const TSharedRef<ISkeletalMeshEditor>& InOwningEditor);

	// Called to attach a toolkit to this toolbox widget (e.g. when the mode changes).
	void AttachToolkit(const TSharedRef<IToolkit>& InToolkit);

	// Called to detach a toolkit from this toolbox widget (e.g. when the mode changes).
	void DetachToolkit(const TSharedRef<IToolkit>& InToolkit);

	// Sets the owning tab so that we can update the tab icon based on what the toolkit provides.
	void SetOwningTab(TSharedRef<SDockTab>& InOwningTab);

private:	
	void UpdateInlineContent(const TSharedPtr<IToolkit>& Toolkit, TSharedPtr<SWidget> InlineContent);

	void UpdatePalette(const TSharedRef<FModeToolkit>& InModeToolkit);

	// Whether to show the inline content or not.
	EVisibility GetInlineContentHolderVisibility() const;

	// The owning tab where the toolkit's interface is hosted. We need this to update the icons
	// and title as the toolkit changes.
	TWeakPtr<SDockTab> OwningTab;

	TWeakPtr<ISkeletalMeshEditor> SkeletalMeshEditor;

	// Inline content area for editor modes
	TSharedPtr<SBorder> InlineContentHolder;

	// The container holding the mode toolbar
	TSharedPtr<SBorder> ModeToolBarContainer;

	// The active tool header area
	TSharedPtr<SBorder> ModeToolHeader;

	// The global status bar to show the name of the active tool.
	FStatusBarMessageHandle StatusBarMessageHandle;
};