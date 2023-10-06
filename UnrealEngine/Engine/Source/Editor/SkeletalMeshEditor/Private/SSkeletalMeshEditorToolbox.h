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


class SSkeletalMeshEditorToolbox : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSkeletalMeshEditorToolbox ) {}
	SLATE_END_ARGS()

	virtual ~SSkeletalMeshEditorToolbox() override = default;

	void Construct(const FArguments& InArgs, const TSharedRef<ISkeletalMeshEditor>& InOwningEditor);

	// Called to attach a toolkit to this toolbox widget (e.g. when the mode changes).
	void AttachToolkit(const TSharedRef<IToolkit>& InToolkit);

	// Called to detach a toolkit from this toolbox widget (e.g. when the mode changes).
	void DetachToolkit(const TSharedRef<IToolkit>& InToolkit);

private:
	// Inline content area for FToolkitBuilder contents
	TSharedPtr<SBorder> InlineContentHolder;
};