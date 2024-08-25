// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDViewportToolbar.h"

#include "ChaosVDEditorSettings.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyEditorModule.h"
#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"
#include "Widgets/SChaosVDEditorViewportViewMenu.h"
#include "Widgets/SChaosVDPlaybackViewport.h"
#include "Widgets/SChaosVDVisualizationControls.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
}

TSharedRef<SEditorViewportViewMenu> SChaosVDViewportToolbar::MakeViewMenu()
{
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();
	return SNew(SChaosVDEditorViewportViewMenu, ViewportRef, SharedThis(this));
}

void SChaosVDViewportToolbar::ExtendOptionsMenu(FMenuBuilder& OptionsMenuBuilder) const
{
	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CVDOptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, GetInfoProvider().GetViewportWidget()->GetCommandList());

	TSharedRef<SVerticalBox> OptionsMenuWidget = SNew(SVerticalBox)
													+SVerticalBox::Slot()
													.Padding(5.0f)
													[
														SNew(STextBlock)
														.Text(LOCTEXT("Default Options Message", "Nothing to see yet. An options menu will be implemented soon"))
													];

	// Intentionally leaving the label empty as we only want to show the temp message defined above
	CVDOptionsMenuBuilder.AddWidget(OptionsMenuWidget, FText());

	OptionsMenuBuilder = CVDOptionsMenuBuilder;
}

TSharedRef<SWidget> SChaosVDViewportToolbar::GenerateShowMenu() const
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsPanel->SetObject(GetMutableDefault<UChaosVDEditorSettings>());

	TSharedRef<SChaosVDPlaybackViewport> ViewportRef = StaticCastSharedRef<SChaosVDPlaybackViewport>(GetInfoProvider().GetViewportWidget());

	return SNew(SChaosVDVisualizationControls, ViewportRef->GetObservedController(), ViewportRef);
}
#undef LOCTEXT_NAMESPACE
