// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowComponentSelectionState.h"

class FDataflowEditorToolkit;
class ADataflowActor;

// ----------------------------------------------------------------------------------

class FDataflowEditorViewportClient : public FEditorViewportClient
{
public:
	using Super = FEditorViewportClient;
	
	FDataflowEditorViewportClient(FPreviewScene* InPreviewScene,
		const TWeakPtr<SEditorViewport> InEditorViewportWidget = nullptr,
		TWeakPtr<FDataflowEditorToolkit> InDataflowEditorToolkitPtr = nullptr);

	Dataflow::FTimestamp LatestTimestamp(const UDataflow* Dataflow, const Dataflow::FContext* Context);
	void SetDataflowActor(ADataflowActor* InActor) { DataflowActor = InActor; }

	void SetSelectionMode(FDataflowSelectionState::EMode InState);
	bool CanSetSelectionMode(FDataflowSelectionState::EMode InState);
	bool IsSelectionModeActive(FDataflowSelectionState::EMode InState);
	FDataflowSelectionState::EMode GetSelectionMode() const { return SelectionMode; }


	// FEditorViewportClient interface
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual void Tick(float DeltaSeconds) override;
	// End of FEditorViewportClient

	//~ FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FDataflowEditorViewportClient"); }

private:

	TWeakPtr<FDataflowEditorToolkit> DataflowEditorToolkitPtr = nullptr;
	Dataflow::FTimestamp LastModifiedTimestamp = Dataflow::FTimestamp::Invalid;
	ADataflowActor* DataflowActor = nullptr;

	//
	// Selection
	//

	FDataflowSelectionState::EMode SelectionMode = FDataflowSelectionState::EMode::DSS_Dataflow_None;

};
