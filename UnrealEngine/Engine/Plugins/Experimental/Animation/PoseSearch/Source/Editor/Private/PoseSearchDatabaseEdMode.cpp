// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEdMode.h"
#include "PoseSearchDatabaseViewportClient.h"
#include "PoseSearchDatabaseEditor.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearch/PoseSearch.h"

#include "EngineUtils.h"

namespace UE::PoseSearch
{
	const FEditorModeID FDatabaseEdMode::EdModeId = TEXT("PoseSearchDatabaseEdMode");

	FDatabaseEdMode::FDatabaseEdMode()
	{
	}

	FDatabaseEdMode::~FDatabaseEdMode()
	{
	}

	void FDatabaseEdMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
	{
		FEdMode::Tick(ViewportClient, DeltaTime);

		FDatabaseViewportClient* PoseSearchDbViewportClient =
			static_cast<FDatabaseViewportClient*>(ViewportClient);
		if (PoseSearchDbViewportClient)
		{
			// ensure we redraw even if PIE is active
			PoseSearchDbViewportClient->Invalidate();

			if (!ViewModel)
			{
				ViewModel = PoseSearchDbViewportClient->GetAssetEditor()->GetViewModel();
			}
		}

		if (ViewModel)
		{
			ViewModel->Tick(DeltaTime);
		}
	}

	void FDatabaseEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
	{
		FEdMode::Render(View, Viewport, PDI);
	}

	bool FDatabaseEdMode::HandleClick(
		FEditorViewportClient* InViewportClient,
		HHitProxy* HitProxy,
		const FViewportClick& Click)
	{
		if (HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy))
		{
			if (ViewModel && IsValid(ActorHitProxy->Actor))
			{
				ViewModel->ProcessSelectedActor(ActorHitProxy->Actor);
				return true;
			}
		}

		ViewModel->ProcessSelectedActor(nullptr);

		return false; // unhandled
	}


	bool FDatabaseEdMode::StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
	{
		return FEdMode::StartTracking(InViewportClient, InViewport);
	}

	bool FDatabaseEdMode::EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport)
	{
		return FEdMode::EndTracking(InViewportClient, InViewport);
	}

	bool FDatabaseEdMode::InputDelta(
		FEditorViewportClient* InViewportClient, 
		FViewport* InViewport, 
		FVector& InDrag, 
		FRotator& InRot, 
		FVector& InScale)
	{
		return FEdMode::InputDelta(InViewportClient, InViewport, InDrag, InRot, InScale);
	}

	bool FDatabaseEdMode::InputKey(
		FEditorViewportClient* ViewportClient, 
		FViewport* Viewport, 
		FKey Key, 
		EInputEvent Event)
	{
		return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
	}

	bool FDatabaseEdMode::AllowWidgetMove()
	{
		return FEdMode::ShouldDrawWidget();
	}

	bool FDatabaseEdMode::ShouldDrawWidget() const
	{
		return FEdMode::ShouldDrawWidget();
	}

	bool FDatabaseEdMode::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
	{
		return FEdMode::GetCustomDrawingCoordinateSystem(InMatrix, InData);
	}

	bool FDatabaseEdMode::GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData)
	{
		return FEdMode::GetCustomDrawingCoordinateSystem(InMatrix, InData);
	}

	FVector FDatabaseEdMode::GetWidgetLocation() const
	{
		return FEdMode::GetWidgetLocation();
	}
}
