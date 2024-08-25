// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorViewport.h"

#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorViewportClient.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "EditorModeManager.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorViewportToolbar.h"
#include "Dataflow/DataflowPreviewScene.h"
#include "Dataflow/DataflowSimulationPanel.h"

#define LOCTEXT_NAMESPACE "SDataflowEditorViewport"


SDataflowEditorViewport::SDataflowEditorViewport()
{
}

void SDataflowEditorViewport::Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs)
{
	SAssetEditorViewport::FArguments ParentArgs;
	ParentArgs._EditorViewportClient = InArgs._ViewportClient;
	SAssetEditorViewport::Construct(ParentArgs, InViewportConstructionArgs);
	Client->VisibilityDelegate.BindSP(this, &SDataflowEditorViewport::IsVisible);

	if(static_cast<FDataflowPreviewScene*>(Client->GetPreviewScene())->CanRunSimulation())
	{
		TSharedPtr<FDataflowEditorViewportClient> DataflowClient = StaticCastSharedPtr<FDataflowEditorViewportClient>(Client);
		TWeakPtr<FDataflowSimulationScene> SimulationScene = DataflowClient->GetDataflowEditorToolkit().Pin()->GetSimulationScene();
            
		ViewportOverlay->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			.FillWidth(1)
			.Padding(10.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
				.Visibility(EVisibility::Visible)
				.Padding(10.0f, 2.0f)
				[
					SNew(SDataflowSimulationPanel, SimulationScene)
					.ViewInputMin(this, &SDataflowEditorViewport::GetViewMinInput)
					.ViewInputMax(this, &SDataflowEditorViewport::GetViewMaxInput)
				]
			]
		];
	}
}

TSharedPtr<SWidget> SDataflowEditorViewport::MakeViewportToolbar()
{
	return SNew(SDataflowViewportSelectionToolBar, SharedThis(this));
}

void SDataflowEditorViewport::OnFocusViewportToSelection()
{
	if(const FDataflowPreviewScene* PreviewScene = static_cast<FDataflowPreviewScene*>(Client->GetPreviewScene()))
	{
		const FBox SceneBoundingBox = PreviewScene->GetBoundingBox();
		Client->FocusViewportOnBox(SceneBoundingBox);
	}
}

UDataflowEditorMode* SDataflowEditorViewport::GetEdMode() const
{
	if (const FEditorModeTools* const EditorModeTools = Client->GetModeTools())
	{
		if (UDataflowEditorMode* const DataflowEdMode = Cast<UDataflowEditorMode>(EditorModeTools->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId)))
		{
			return DataflowEdMode;
		}
	}
	return nullptr;
}

void SDataflowEditorViewport::BindCommands()
{
	SAssetEditorViewport::BindCommands();
}

bool SDataflowEditorViewport::IsVisible() const
{
	// Intentionally not calling SEditorViewport::IsVisible because it will return false if our simulation is more than 250ms.
	return ViewportWidget.IsValid();
}

TSharedRef<class SEditorViewport> SDataflowEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SDataflowEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SDataflowEditorViewport::OnFloatingButtonClicked()
{
}

float SDataflowEditorViewport:: GetViewMinInput() const
{
	return static_cast<FDataflowPreviewScene*>(Client->GetPreviewScene())->GetDataflowContent()->GetSimulationRange()[0];
}

float SDataflowEditorViewport::GetViewMaxInput() const
{
	return static_cast<FDataflowPreviewScene*>(Client->GetPreviewScene())->GetDataflowContent()->GetSimulationRange()[1];
}


#undef LOCTEXT_NAMESPACE
