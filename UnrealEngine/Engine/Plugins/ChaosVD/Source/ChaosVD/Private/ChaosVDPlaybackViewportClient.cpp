// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDPlaybackViewportClient.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDEngine.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDScene.h"
#include "ChaosVDSkySphereInterface.h"
#include "Components/ChaosVDSceneQueryDataComponent.h"
#include "ComponentVisualizer.h"
#include "EditorModeManager.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "SceneView.h"
#include "SEditorViewport.h"
#include "Selection.h"
#include "UnrealWidget.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Components/ChaosVDParticleDataComponent.h"
#include "Components/ChaosVDSolverCollisionDataComponent.h"
#include "Components/ChaosVDSolverJointConstraintDataComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"
#include "Widgets/SChaosVDMainTab.h"

FChaosVDPlaybackViewportClient::FChaosVDPlaybackViewportClient(const TSharedPtr<FEditorModeTools>& InModeTools, const TSharedPtr<SEditorViewport>& InEditorViewportWidget) : FEditorViewportClient(InModeTools.Get(), nullptr, InEditorViewportWidget), CVDWorld(nullptr)
{
	Widget->SetUsesEditorModeTools(InModeTools.Get());

	if (GEngine)
	{
		GEngine->OnActorMoving().AddRaw(this, &FChaosVDPlaybackViewportClient::HandleActorMoving);
	}

	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->OnFarClippingOverrideChanged().AddRaw(this, &FChaosVDPlaybackViewportClient::HandleViewportSettingsChanged);
		Settings->OnVisibilitySettingsChanged().AddRaw(this, &FChaosVDPlaybackViewportClient::HandleViewportSettingsChanged);

		HandleViewportSettingsChanged(Settings);
	}
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

	if (GEngine)
	{
		GEngine->OnActorMoving().RemoveAll(this);
	}
	
	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->OnFarClippingOverrideChanged().RemoveAll(this);
		Settings->OnVisibilitySettingsChanged().RemoveAll(this);
	}
}

void FChaosVDPlaybackViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);

	if (HitProxy == nullptr)
	{
		return;
	}
	
	const TSharedPtr<SChaosVDMainTab> MainTabToolkitHost = ModeTools.IsValid() ? StaticCastSharedPtr<SChaosVDMainTab>(ModeTools->GetToolkitHost()) : nullptr;
	if (!MainTabToolkitHost.IsValid())
	{
		return;
	}

	const FViewportClick Click(&View, this, Key, Event, HitX, HitY);

	if (const TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{
		bool bClickHandled = false;

		HComponentVisProxy* ComponentVisProxy = HitProxyCast<HComponentVisProxy>(HitProxy);
		const TConstArrayView<TSharedPtr<FComponentVisualizer>> AllVisualizers = MainTabToolkitHost->GetAllComponentVisualizers();
		for (const TSharedPtr<FComponentVisualizer>& Visualizer : AllVisualizers)
		{
			// Not sure if this is compliant with the normal use of the component visualizers,
			// but passing a null hitproxy when the hit proxy was not a component
			// It allow us to handle things like clear selection on the Collision Data Visualizer
			if (Visualizer->VisProxyHandleClick(this, ComponentVisProxy, Click))
			{
				bClickHandled = true;
				break;
			}
		}

		if (bClickHandled)
		{
			return;
		}

		const IChaosVDGeometryComponent* AsCVDGeometryComponent = nullptr;
		int32 MeshInstanceIndex = INDEX_NONE;

		if (const HInstancedStaticMeshInstance* InstancedStaticMeshProxy = HitProxyCast<HInstancedStaticMeshInstance>(HitProxy))
		{
			AsCVDGeometryComponent = Cast<IChaosVDGeometryComponent>(InstancedStaticMeshProxy->Component);
			MeshInstanceIndex = InstancedStaticMeshProxy->InstanceIndex;
		}
		else if (const HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy))
		{
			AsCVDGeometryComponent = Cast<IChaosVDGeometryComponent>(ActorHitProxy->PrimComponent.Get());
			MeshInstanceIndex = 0;
		}

		if (AsCVDGeometryComponent && MeshInstanceIndex != INDEX_NONE)
		{
			if (const TSharedPtr<FChaosVDMeshDataInstanceHandle> MeshDataHandle = AsCVDGeometryComponent->GetMeshDataInstanceHandle(MeshInstanceIndex))
			{
				if (AChaosVDParticleActor* ClickedActor = ScenePtr->GetParticleActor(MeshDataHandle->GetOwningSolverID(), MeshDataHandle->GetOwningParticleID()))
				{
					ScenePtr->SetSelectedObject(ClickedActor);
					bClickHandled = true;
				}
			}
		}

		if (bClickHandled)
		{
			return;
		}

		if (HitProxy->IsA(HActor::StaticGetType()))
		{
			const HActor* ActorHitProxy = static_cast<HActor*>(HitProxy);
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

void FChaosVDPlaybackViewportClient::HandleActorMoving(AActor* MovedActor) const
{
	if (Cast<ADirectionalLight>(MovedActor))
	{
		if (const TSharedPtr<FChaosVDScene> SceneSharedPtr = CVDScene.Pin())
		{
			if (SceneSharedPtr->GetSkySphereActor()->Implements<UChaosVDSkySphereInterface>())
			{
				IChaosVDSkySphereInterface::Execute_Refresh(SceneSharedPtr->GetSkySphereActor());
			}
		}
	}
}

void FChaosVDPlaybackViewportClient::HandleViewportSettingsChanged(UChaosVDEditorSettings* SettingsObject)
{
	if (SettingsObject)
	{
		OverrideFarClipPlane(SettingsObject->FarClippingOverride);
		EngineShowFlags.SetMeshEdges(EnumHasAnyFlags(static_cast<EChaosVDGeometryVisibilityFlags>(SettingsObject->GeometryVisibilityFlags), EChaosVDGeometryVisibilityFlags::ShowTriangleEdges));
		Invalidate();
	}
}

void FChaosVDPlaybackViewportClient::TrackSelectedObject()
{
	if (const TSharedPtr<FChaosVDScene> CVDSceneSharedPtr = CVDScene.Pin())
	{
		if (const UChaosVDEditorSettings* CVDEditorSettings = GetDefault<UChaosVDEditorSettings>())
		{
			if (ModeTools.IsValid() && CVDEditorSettings->TrackingTarget == EChaosVDActorTrackingTarget::SelectedObject)
			{
				USelection* CurrentSelection = ModeTools->GetSelectedActors();

				//TODO: Update this if we add multi selection support
				if (const AActor* SelectedActor = CurrentSelection ? CurrentSelection->GetTop<AActor>() : nullptr)
				{
					const FBox ActorBounds = SelectedActor->GetComponentsBoundingBox(false);
					FocusViewportOnBox(ActorBounds.ExpandBy(CVDEditorSettings->ExpandViewTrackingBy), true);		
				}
			}
		}
	}
}

bool FChaosVDPlaybackViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	// Each time we requested a re-draw means we move something in the scene, so we need to re-create the cached hit proxy map.
	if (bNeedsRedraw)
	{
		RequestInvalidateHitProxy(Viewport);
	}
	
	return FEditorViewportClient::InputKey(EventArgs);
}

void FChaosVDPlaybackViewportClient::ToggleObjectTrackingIfSelected()
{
	// Currently we only have two options, so toggle between them
	if (UChaosVDEditorSettings* CVDEditorSettings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		CVDEditorSettings->TrackingTarget = CVDEditorSettings->TrackingTarget == EChaosVDActorTrackingTarget::Disabled ? EChaosVDActorTrackingTarget::SelectedObject : EChaosVDActorTrackingTarget::Disabled;
	}
}

void FChaosVDPlaybackViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (View)
	{
		// Hack to allow selection of translucent objects (for CVD is all geometry set a Query Only)
		// The current setting to allow this behaviour is project wide or on custom hitproxies implementations which we can't use
		// A proper fix would be have a way to override this per viewport, which could be done by adding a new method to FViewElementDrawer
		const_cast<FSceneView*>(View)->bAllowTranslucentPrimitivesInHitProxy = true;
	}

	TrackSelectedObject();

	const TSharedPtr<SChaosVDMainTab> MainTabToolkitHost = ModeTools.IsValid() ? StaticCastSharedPtr<SChaosVDMainTab>(ModeTools->GetToolkitHost()) : nullptr;
	if (!MainTabToolkitHost.IsValid())
	{
		return;
	}

	if (const TSharedPtr<FChaosVDScene> ScenePtr = CVDScene.Pin())
	{
		//TODO: Currently we can safely assume that any component in these actors is meant to have a visualizer, but we might need a proper interface for these components in the future
		TInlineComponentArray<const UActorComponent*> ComponentsToVisualize;
		for (const TPair<int32, AChaosVDSolverInfoActor*>& SolverInfoWithID : ScenePtr->GetSolverInfoActorsMap())
		{
			if (SolverInfoWithID.Value)
			{
				constexpr bool bIncludeFromChildActors = false;
				SolverInfoWithID.Value->ForEachComponent(bIncludeFromChildActors, [&ComponentsToVisualize](UActorComponent* Component)
				{
					ComponentsToVisualize.Emplace(Component);
				});
			}
		}

		if (const UChaosVDSceneQueryDataComponent* SceneQueryDataComponent = ScenePtr->GetSceneQueryDataContainerComponent())
		{
			ComponentsToVisualize.Emplace(SceneQueryDataComponent);
		}

		for (const UActorComponent* Component : ComponentsToVisualize)
		{
			if (!FChaosVDDebugDrawUtils::CanDebugDraw())
			{
				break;
			}

			if (const TSharedPtr<FComponentVisualizer> Visualizer = MainTabToolkitHost->FindComponentVisualizer(Component->GetClass()))
			{
				Visualizer->DrawVisualization(Component, View, PDI);
			}
		}
	}

	FEditorViewportClient::Draw(View, PDI);
	
	FChaosVDDebugDrawUtils::DebugDrawFrameEnd();
}

void FChaosVDPlaybackViewportClient::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas)
{
	FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);

	FChaosVDDebugDrawUtils::DrawCanvas(InViewport, View, Canvas);
}
