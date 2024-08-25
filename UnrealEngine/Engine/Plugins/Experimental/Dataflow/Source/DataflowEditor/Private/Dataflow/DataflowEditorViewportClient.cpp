// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowEditorViewportClient.h"

#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEngineSceneHitProxies.h"
#include "Dataflow/DataflowPreviewScene.h"
#include "EditorModeManager.h"
#include "PreviewScene.h"
#include "Selection.h"

FDataflowEditorViewportClient::FDataflowEditorViewportClient(FEditorModeTools* InModeTools,
                                                             FPreviewScene* InPreviewScene,  const bool bCouldTickScene,
                                                             const TWeakPtr<SEditorViewport> InEditorViewportWidget)
	: FEditorViewportClient(InModeTools, InPreviewScene, InEditorViewportWidget)
{
	// We want our near clip plane to be quite close so that we can zoom in further.
	OverrideNearClipPlane(KINDA_SMALL_NUMBER);

	EngineShowFlags.SetSelectionOutline(true);
	EngineShowFlags.EnableAdvancedFeatures();

	PreviewScene = static_cast<FDataflowPreviewScene*>(InPreviewScene);
	bEnableSceneTicking = bCouldTickScene;
}

void FDataflowEditorViewportClient::SetDataflowEditorToolkit(TWeakPtr<FDataflowEditorToolkit> InDataflowEditorToolkitPtr)
{
	DataflowEditorToolkitPtr = InDataflowEditorToolkitPtr;
}

void FDataflowEditorViewportClient::SetToolCommandList(TWeakPtr<FUICommandList> InToolCommandList)
{
	ToolCommandList = InToolCommandList;
}

//const UInputBehaviorSet* FDataflowEditorViewportClient::GetInputBehaviors() const
//{
//	return BehaviorSet;
//}

void FDataflowEditorViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (PreviewScene)
	{
		PreviewScene->TickDataflowScene(DeltaSeconds);
	}
}

void FDataflowEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	Super::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);

	USelection* SelectedComponents = ModeTools->GetSelectedComponents();

	TArray<UPrimitiveComponent*> PreviouslySelectedComponents;
	SelectedComponents->GetSelectedObjects<UPrimitiveComponent>(PreviouslySelectedComponents);

	SelectedComponents->Modify();
	SelectedComponents->BeginBatchSelectOperation();

	SelectedComponents->DeselectAll();

	if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
	{
		const HActor* ActorProxy = static_cast<HActor*>(HitProxy);
		if (ActorProxy && ActorProxy->PrimComponent && ActorProxy->Actor)
		{
			UPrimitiveComponent* Component = const_cast<UPrimitiveComponent*>(ActorProxy->PrimComponent.Get());
			SelectedComponents->Select(Component);
			Component->PushSelectionToProxy();
		}
	}

	SelectedComponents->EndBatchSelectOperation();

	for (UPrimitiveComponent* const Component : PreviouslySelectedComponents)
	{
		Component->PushSelectionToProxy();
	}
}

void FDataflowEditorViewportClient::SetConstructionViewMode(Dataflow::EDataflowPatternVertexType InViewMode)
{
	// @todo(Dataflow) : Add support for Sim2D
	//const bool bSwitching2D3D = (ConstructionViewMode == Dataflow::EDataflowPatternVertexType::Sim2D) != (InViewMode == Dataflow::EDataflowPatternVertexType::Sim2D);
	//if (bSwitching2D3D)
	//{
	//	Swap(SavedInactiveViewTransform, ViewTransformPerspective);
	//}

	ConstructionViewMode = Dataflow::EDataflowPatternVertexType::Sim3D;

	
	//if (ConstructionViewMode == EDataflowPatternVertexType::Sim2D)
	//{
	//	for (UInputBehavior* const Behavior : BehaviorsFor2DMode)
	//	{
	//		BehaviorSet->Add(Behavior);
	//	}
	//
	//	const double AbsZ = FMath::Abs(ViewTransformPerspective.GetLocation().Z);
	//	constexpr double CameraFarPlaneWorldZ = -10.0;
	//	constexpr double CameraNearPlaneProportionZ = 0.8;
	//	OverrideFarClipPlane(static_cast<float>(AbsZ - CameraFarPlaneWorldZ));
	//	OverrideNearClipPlane(static_cast<float>(AbsZ * (1.0 - CameraNearPlaneProportionZ)));
	//}
	//else
	//{
	OverrideFarClipPlane(0);
	OverrideNearClipPlane(UE_KINDA_SMALL_NUMBER);
	//}

	//ModeTools->GetInteractiveToolsContext()->InputRouter->DeregisterSource(this);
	//ModeTools->GetInteractiveToolsContext()->InputRouter->RegisterSource(this);

}


Dataflow::EDataflowPatternVertexType FDataflowEditorViewportClient::GetConstructionViewMode() const
{
	return Dataflow::EDataflowPatternVertexType::Sim3D;// ConstructionViewMode;
}

void FDataflowEditorViewportClient::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(BehaviorSet);
}

