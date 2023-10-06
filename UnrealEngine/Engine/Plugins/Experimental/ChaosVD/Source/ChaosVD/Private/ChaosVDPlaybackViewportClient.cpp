// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDPlaybackViewportClient.h"

#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "EngineUtils.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"


FChaosVDPlaybackViewportClient::FChaosVDPlaybackViewportClient() : FEditorViewportClient(nullptr), CVDWorld(nullptr)
{
}

FChaosVDPlaybackViewportClient::~FChaosVDPlaybackViewportClient()
{
	if (ObjectFocusedDelegateHandle.IsValid())
	{
		if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
		{
			ScenePtr->OnObjectFocused().Remove(ObjectFocusedDelegateHandle);
		}
	}
}

void FChaosVDPlaybackViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	if (HitProxy == nullptr)
	{
		return;
	}

	if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{
		if (HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorHitProxy = static_cast<HActor*>(HitProxy);
			if (AActor* ClickedActor = ActorHitProxy->Actor)
			{
				ScenePtr->SetSelectedObject(ClickedActor);
			}
		}
	}
}

void FChaosVDPlaybackViewportClient::SetScene(TWeakPtr<FChaosVDScene> InScene)
{
	if (TSharedPtr<FChaosVDScene> ScenePtr = InScene.Pin())
	{
		CVDWorld = ScenePtr->GetUnderlyingWorld();
		CVDScene = InScene;

		ObjectFocusedDelegateHandle = ScenePtr->OnObjectFocused().AddRaw(this, &FChaosVDPlaybackViewportClient::HandleObjectFocused);
	}
}

void FChaosVDPlaybackViewportClient::HandleObjectFocused(UObject* FocusedObject)
{
	if (AActor* FocusedActor = Cast<AActor>(FocusedObject))
	{
		FocusViewportOnBox(FocusedActor->GetComponentsBoundingBox(false));
	}
}

void FChaosVDPlaybackViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	if (TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{	
		TArray<AActor*> SelectedActors = ScenePtr->GetElementSelectionSet()->GetSelectedObjects<AActor>();

		for (AActor* SelectedActor : SelectedActors)
		{
			if (IChaosVDVisualizerContainerInterface* VisualizerContainer = Cast<IChaosVDVisualizerContainerInterface>(SelectedActor))
			{
				VisualizerContainer->DrawVisualization(View, PDI);
			}
		}
	}
}

void FChaosVDPlaybackViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	FChaosVDDebugDrawUtils::DrawCanvas(InViewport, View, Canvas);
}
