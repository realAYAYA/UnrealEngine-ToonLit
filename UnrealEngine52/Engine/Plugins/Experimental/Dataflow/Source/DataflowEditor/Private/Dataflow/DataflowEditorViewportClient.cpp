// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowEditorViewportClient.h"

#include "Dataflow/DataflowActor.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEngineSceneHitProxies.h"
#include "Dataflow/DataflowXml.h"
#include "HAL/PlatformApplicationMisc.h"
#include "PreviewScene.h"

FDataflowEditorViewportClient::FDataflowEditorViewportClient(FPreviewScene* InPreviewScene, 
	const TWeakPtr<SEditorViewport> InEditorViewportWidget,
	TWeakPtr<FDataflowEditorToolkit> InDataflowEditorToolkitPtr)
	: 
	FEditorViewportClient(nullptr, InPreviewScene, InEditorViewportWidget)
	, DataflowEditorToolkitPtr(InDataflowEditorToolkitPtr)
{
	SetRealtime(true);
	SetViewModes(VMI_Lit, VMI_Lit);
	bSetListenerPosition = false;
	EngineShowFlags.Grid = false;
}

void FDataflowEditorViewportClient::SetSelectionMode(FDataflowSelectionState::EMode InState)
{
	FDataflowSelectionState State = DataflowActor->DataflowComponent->GetSelectionState();
		
	if (SelectionMode == InState)
	{
		SelectionMode = FDataflowSelectionState::EMode::DSS_Dataflow_None;
	}
	else
	{
		SelectionMode = InState;
	}

	State.Mode = SelectionMode;
	DataflowActor->DataflowComponent->SetSelectionState(State);

	if (SelectionMode == FDataflowSelectionState::EMode::DSS_Dataflow_None)
	{
		if (!DataflowActor->DataflowComponent->GetSelectionState().IsEmpty())
		{
			DataflowActor->DataflowComponent->SetSelectionState(FDataflowSelectionState(SelectionMode));
		}
	}
}
bool FDataflowEditorViewportClient::CanSetSelectionMode(FDataflowSelectionState::EMode InState)
{
	TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin();
	if (DataflowEditorToolkitPtr.IsValid())
	{
		if (const UDataflow* Dataflow = DataflowEditorToolkit->GetDataflow())
		{
			if (Dataflow->GetRenderTargets().Num())
			{
				if (InState == FDataflowSelectionState::EMode::DSS_Dataflow_Object)
				{
					return true;
				}

				if (InState == FDataflowSelectionState::EMode::DSS_Dataflow_Vertex
					&& !DataflowActor->DataflowComponent->GetSelectionState().Nodes.IsEmpty())
				{
					return true;
				}
			}
		}
	}

	return false;
}
bool FDataflowEditorViewportClient::IsSelectionModeActive(FDataflowSelectionState::EMode InState) 
{ 
	return SelectionMode == InState;
}

Dataflow::FTimestamp FDataflowEditorViewportClient::LatestTimestamp(const UDataflow* Dataflow, const Dataflow::FContext* Context)
{
	if (Dataflow && Context)
	{
		return FMath::Max(Dataflow->GetRenderingTimestamp().Value, Context->GetTimestamp().Value);
	}
	return Dataflow::FTimestamp::Invalid;
}

bool FDataflowEditorViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	bool bHandled = false;

	FInputEventState InputState(EventArgs.Viewport, EventArgs.Key, EventArgs.Event);
	if (InputState.IsCtrlButtonPressed() && EventArgs.Key == EKeys::C)
	{
		if (DataflowActor->DataflowComponent->GetSelectionState().Vertices.Num())
		{
			FString XmlBuffer = FDataflowXmlWrite()
				.Begin()
				.MakeVertexSelectionBlock(DataflowActor->DataflowComponent->GetSelectionState().Vertices)
				.End()
				.ToString();
			FPlatformApplicationMisc::ClipboardCopy(*XmlBuffer);
		}
	}


	if (!bHandled)
	{
		bHandled = FEditorViewportClient::InputKey(EventArgs);
	}

	return bHandled;
}

void FDataflowEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	Super::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
	if (DataflowActor && DataflowActor->DataflowComponent)
	{
		const bool bIsShiftKeyDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);
		const bool bIsCtrltKeyDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);

		FDataflowSelectionState SelectionState = DataflowActor->DataflowComponent->GetSelectionState();
		FDataflowSelectionState PreState = SelectionState;

		if (SelectionMode == FDataflowSelectionState::EMode::DSS_Dataflow_Object)
		{
			if (HitProxy && HitProxy->IsA(HDataflowNode::StaticGetType()))
			{
				HDataflowNode* DataflowNode = (HDataflowNode*)(HitProxy);
				FDataflowSelectionState::ObjectID ID(DataflowNode->NodeName, DataflowNode->GeometryIndex);

				if (bIsShiftKeyDown)
				{
					if (!SelectionState.Nodes.Contains(ID))
					{
						SelectionState.Nodes.AddUnique(ID);
					}
				}
				else if (bIsCtrltKeyDown)
				{
					if (SelectionState.Nodes.Contains(ID))
					{
						SelectionState.Nodes.Remove(ID);
					}
				}
				else
				{
					SelectionState.Nodes.Empty();
					SelectionState.Nodes.AddUnique(ID);
				}
			}
			else if (!bIsShiftKeyDown && !bIsCtrltKeyDown)
			{
				SelectionState.Nodes.Empty();
			}
		}

		if (SelectionMode == FDataflowSelectionState::EMode::DSS_Dataflow_Vertex)
		{
			if (HitProxy && HitProxy->IsA(HDataflowVertex::StaticGetType()))
			{
				HDataflowVertex* DataflowVertex = (HDataflowVertex*)(HitProxy);
				int32 ID = DataflowVertex->SectionIndex;
				if (bIsShiftKeyDown)
				{
					if (!SelectionState.Vertices.Contains(ID))
					{
						SelectionState.Vertices.AddUnique(ID);
					}
				}
				else if (bIsCtrltKeyDown)
				{
					if (SelectionState.Vertices.Contains(ID))
					{
						SelectionState.Vertices.Remove(ID);
					}
				}
				else
				{
					SelectionState.Vertices.Empty();
					SelectionState.Vertices.AddUnique(ID);
				}
			}
			else if (!bIsShiftKeyDown && !bIsCtrltKeyDown)
			{
				SelectionState.Vertices.Empty();
			}
		}

		if (PreState != SelectionState)
		{
			DataflowActor->DataflowComponent->SetSelectionState(SelectionState);
		}
	}
}

void FDataflowEditorViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TSharedPtr<FDataflowEditorToolkit> DataflowEditorToolkit = DataflowEditorToolkitPtr.Pin();

	if (DataflowActor && DataflowEditorToolkitPtr.IsValid())
	{
		if (TSharedPtr<Dataflow::FContext> Context = DataflowEditorToolkit->GetContext())
		{
			if (const UDataflow* Dataflow = DataflowEditorToolkit->GetDataflow())
			{
				if (UDataflowComponent* DataflowComponent = DataflowActor->GetDataflowComponent())
				{
					Dataflow::FTimestamp SystemTimestamp = LatestTimestamp(Dataflow, Context.Get());
					if (SystemTimestamp >= LastModifiedTimestamp)
					{
						if (Dataflow->GetRenderTargets().Num())
						{
							// Component Object Rendering
							DataflowComponent->ResetRenderTargets();
							DataflowComponent->SetDataflow(Dataflow);
							DataflowComponent->SetContext(Context);
							for (const UDataflowEdNode* Node : Dataflow->GetRenderTargets())
							{
								DataflowComponent->AddRenderTarget(Node);
							}
						}
						else
						{
							DataflowComponent->ResetRenderTargets();
						}

						LastModifiedTimestamp = LatestTimestamp(Dataflow, Context.Get()).Value + 1;
					}
				}
			}
		}
	}

	// Tick the preview scene world.
	PreviewScene->GetWorld()->Tick(IsRealtime() ? LEVELTICK_All : LEVELTICK_TimeOnly, DeltaSeconds);
}


void FDataflowEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}


