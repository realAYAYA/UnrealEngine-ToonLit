// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionAppMode.h"
#include "AvaTransitionEditor.h"
#include "TabFactories/AvaTransitionCompilerResultsTabFactory.h"
#include "TabFactories/AvaTransitionSelectionDetailsTabFactory.h"
#include "TabFactories/AvaTransitionTreeTabFactory.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "WorkflowOrientedApp/SModeWidget.h"

#define LOCTEXT_NAMESPACE "AvaTransitionAppMode"

namespace UE::AvaTransitionEditor::Private
{
	static const TMap<EAvaTransitionEditorMode, FName> GModeNames
	{
		{ EAvaTransitionEditorMode::Default , TEXT("Default") },
		{ EAvaTransitionEditorMode::Advanced, TEXT("Advanced") },
	};

	static const TMap<FName, FText> GLocalizedModeNames
	{
		{ TEXT("Default") , LOCTEXT("Default", "Default") },
		{ TEXT("Advanced"), LOCTEXT("Advanced", "Advanced") },
	};

	FText GetLocalizedMode(FName InModeName)
	{
		return GLocalizedModeNames[InModeName];
	}
}

FAvaTransitionAppMode::FAvaTransitionAppMode(const TSharedRef<FAvaTransitionEditor>& InEditor, EAvaTransitionEditorMode InEditorMode)
	: FApplicationMode(UE::AvaTransitionEditor::Private::GModeNames[InEditorMode], &UE::AvaTransitionEditor::Private::GetLocalizedMode)
	, EditorWeak(InEditor)
	, EditorMode(InEditorMode)
{
	// Default Factories
	TabFactories.RegisterFactory(MakeShared<FAvaTransitionTreeTabFactory>(InEditor));
	TabFactories.RegisterFactory(MakeShared<FAvaTransitionSelectionDetailsTabFactory>(InEditor, InEditorMode));
	TabFactories.RegisterFactory(MakeShared<FAvaTransitionCompilerResultsTabFactory>(InEditor));
}

void FAvaTransitionAppMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FAvaTransitionEditor> Editor = EditorWeak.Pin();
	check(Editor.IsValid());
	Editor->SetEditorMode(EditorMode);
	Editor->PushTabFactories(TabFactories);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FAvaTransitionAppMode::AddToToolbar(const TSharedRef<FExtender>& InToolbarExtender)
{
	ToolbarExtender = InToolbarExtender;
	InToolbarExtender->AddToolBarExtension("Asset"
		, EExtensionHook::After
		, nullptr
		, FToolBarExtensionDelegate::CreateSP(this, &FAvaTransitionAppMode::ExtendToolbar));
}

void FAvaTransitionAppMode::ExtendToolbar(FToolBarBuilder& InToolbarBuilder)
{
	if (!EditorWeak.IsValid())
	{
		return;
	}

	TSharedRef<FAvaTransitionEditor> Editor = EditorWeak.Pin().ToSharedRef();

	FText LocalizedModeName = UE::AvaTransitionEditor::Private::GetLocalizedMode(ModeName);

	Editor->AddToolbarWidget(
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.Padding(1)
		[
			SNew(SBox)
			.MinDesiredHeight(24.f)
			[
				SNew(SModeWidget, LocalizedModeName, ModeName)
				.OnGetActiveMode(Editor, &FAvaTransitionEditor::GetCurrentMode)
				.OnSetActiveMode(FOnModeChangeRequested::CreateSP(Editor, &FAvaTransitionEditor::SetCurrentMode))
				.ToolTipText(FText::Format(LOCTEXT("ModeButtonTooltip", "Switch to {0} Mode"), LocalizedModeName))
				.IconImage(ModeIcon)
				.AddMetaData<FTagMetaData>(FTagMetaData(ModeName))
			]
		]
	);
}

#undef LOCTEXT_NAMESPACE
