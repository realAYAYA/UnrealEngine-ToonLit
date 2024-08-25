// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDVisualizationControls.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDPlaybackViewportClient.h"
#include "ChaosVDRecording.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SChaosVDNameListPicker.h"
#include "Widgets/SChaosVDPlaybackViewport.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDVisualizationControls::Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController,const TWeakPtr<SChaosVDPlaybackViewport>& InPlaybackViewport)
{
	RegisterNewController(InPlaybackController);

	PlaybackViewport = InPlaybackViewport;
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	const TSharedRef<IDetailsView> DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsPanel->SetObject(GetMutableDefault<UChaosVDEditorSettings>());

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2,5,2,5)
		.AutoHeight()
		[
			DetailsPanel
		]
	];
}


void SChaosVDVisualizationControls::RegisterCommandsUI()
{
	
}

#undef LOCTEXT_NAMESPACE
