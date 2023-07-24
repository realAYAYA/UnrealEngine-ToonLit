// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowEditorViewport.h"

#include "AdvancedPreviewScene.h"
#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorViewportToolbar.h"
#include "Dataflow/DataflowEditorViewportClient.h"
#include "EditorViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "PreviewScene.h"

SDataflowEditorViewport::SDataflowEditorViewport()
{
	PreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	PreviewScene->SetFloorVisibility(false);
}

void SDataflowEditorViewport::Construct(const FArguments& InArgs)
{
	DataflowEditorToolkitPtr = InArgs._DataflowEditorToolkit;
	TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin();
	check(DataflowEditorToolkitPtr.IsValid());

	SEditorViewport::Construct(SEditorViewport::FArguments());

	FBoxSphereBounds SphereBounds = FBoxSphereBounds(EForceInit::ForceInitToZero);
	CustomDataflowActor = CastChecked<ADataflowActor>(PreviewScene->GetWorld()->SpawnActor(ADataflowActor::StaticClass()));

	ViewportClient->SetDataflowActor(CustomDataflowActor);
	ViewportClient->FocusViewportOnBox( SphereBounds.GetBox());
}

TSharedRef<SEditorViewport> SDataflowEditorViewport::GetViewportWidget()
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

void SDataflowEditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CustomDataflowActor);
}

TSharedRef<FEditorViewportClient> SDataflowEditorViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FDataflowEditorViewportClient(PreviewScene.Get(), SharedThis(this), DataflowEditorToolkitPtr));
	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SDataflowEditorViewport::MakeViewportToolbar()
{
	return
		SNew(SDataflowViewportSelectionToolBar)
		.EditorViewport(SharedThis(this))
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}

void SDataflowEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();
	{
		const FDataflowEditorCommandsImpl& Commands = FDataflowEditorCommands::Get();
		TSharedRef<FDataflowEditorViewportClient> ClientRef = ViewportClient.ToSharedRef();

		CommandList->MapAction(
			Commands.ToggleObjectSelection,
			FExecuteAction::CreateSP(ClientRef, &FDataflowEditorViewportClient::SetSelectionMode, FDataflowSelectionState::EMode::DSS_Dataflow_Object)
			,FCanExecuteAction::CreateSP(ClientRef, &FDataflowEditorViewportClient::CanSetSelectionMode, FDataflowSelectionState::EMode::DSS_Dataflow_Object)
			,FIsActionChecked::CreateSP(ClientRef, &FDataflowEditorViewportClient::IsSelectionModeActive, FDataflowSelectionState::EMode::DSS_Dataflow_Object)
		);

		CommandList->MapAction(
			Commands.ToggleFaceSelection,
			FExecuteAction::CreateSP(ClientRef, &FDataflowEditorViewportClient::SetSelectionMode, FDataflowSelectionState::EMode::DSS_Dataflow_Face)
			, FCanExecuteAction::CreateSP(ClientRef, &FDataflowEditorViewportClient::CanSetSelectionMode, FDataflowSelectionState::EMode::DSS_Dataflow_Face)
			, FIsActionChecked::CreateSP(ClientRef, &FDataflowEditorViewportClient::IsSelectionModeActive, FDataflowSelectionState::EMode::DSS_Dataflow_Face)
		);

		CommandList->MapAction(
			Commands.ToggleVertexSelection,
			FExecuteAction::CreateSP(ClientRef, &FDataflowEditorViewportClient::SetSelectionMode, FDataflowSelectionState::EMode::DSS_Dataflow_Vertex)
			, FCanExecuteAction::CreateSP(ClientRef, &FDataflowEditorViewportClient::CanSetSelectionMode, FDataflowSelectionState::EMode::DSS_Dataflow_Vertex)
			, FIsActionChecked::CreateSP(ClientRef, &FDataflowEditorViewportClient::IsSelectionModeActive, FDataflowSelectionState::EMode::DSS_Dataflow_Vertex)
		);
	}
}
