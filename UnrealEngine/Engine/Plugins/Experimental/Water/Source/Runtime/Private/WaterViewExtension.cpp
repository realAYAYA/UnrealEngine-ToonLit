// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterViewExtension.h"
#include "WaterBodyComponent.h"
#include "WaterZoneActor.h"
#include "EngineUtils.h"
#include "SceneView.h"
#include "WaterMeshComponent.h"

static TAutoConsoleVariable<float> CVarFreezeWaterLODUpdates(
	TEXT("r.Water.FreezeLODUpdates"),
	0,
	TEXT(""),
	ECVF_Default);

// ----------------------------------------------------------------------------------

FWaterViewExtension::FWaterViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld)
	: FWorldSceneViewExtension(AutoReg, InWorld)
{
}

FWaterViewExtension::~FWaterViewExtension()
{
}

void FWaterViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}


void FWaterViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	if (CVarFreezeWaterLODUpdates.GetValueOnGameThread() != 0)
	{
		return;
	}

	const FVector ViewLocation = InView.ViewLocation;

	const TWeakObjectPtr<UWorld> WorldPtr = GetWorld();
	check(WorldPtr.IsValid())

	static bool bUpdatingWaterInfo = false;
	if (!bUpdatingWaterInfo)
	{
		bUpdatingWaterInfo = true;

		for (const TPair<AWaterZone*, UE::WaterInfo::FRenderingContext>& Pair : WaterInfoContextsToRender)
		{
			AWaterZone* WaterZone = Pair.Key;
			check(WaterZone);

			if (WaterZone->IsLocalOnlyTessellationEnabled())
			{
				UWaterMeshComponent* WaterMesh= WaterZone->GetWaterMeshComponent();
				check(WaterMesh);

				const double TileSize = WaterMesh->GetTileSize();

				const FVector WaterMeshCenter = ViewLocation.GridSnap(TileSize);

				const FVector WaterMeshHalfExtent = WaterZone->GetDynamicWaterMeshExtent() / 2.0;
				const FBox2D WaterMeshBounds(FVector2D(WaterMeshCenter - WaterMeshHalfExtent), FVector2D(WaterMeshCenter + WaterMeshHalfExtent));

				WaterZone->SetLocalTessellationCenter(WaterMeshCenter);
				WaterMesh->PushTessellatedWaterMeshBoundsToPoxy(WaterMeshBounds.ExpandBy(2 * TileSize));

				const FBox2D CurrentCellBounds(FVector2D(ViewLocation) - FVector2D(TileSize / 2.0), FVector2D(ViewLocation) + FVector2D(TileSize / 2.0));
				UpdateBounds.Add(WaterZone, CurrentCellBounds);
			}

			// Update the material instances now that the tessellated mesh bounds have changed.
			WaterZone->ForEachWaterBodyComponent([](UWaterBodyComponent* WaterBodyComponent)
			{
				WaterBodyComponent->UpdateMaterialInstances();
				return true;
			});

			const UE::WaterInfo::FRenderingContext& Context(Pair.Value);
			UE::WaterInfo::UpdateWaterInfoRendering(WorldPtr.Get()->Scene, Context);
		}

		WaterInfoContextsToRender.Empty();

		// Check if the view location is no longer within the current update bounds of a water zone and if so, queue an update for it.
		for (AWaterZone* WaterZone : TActorRange<AWaterZone>(WorldPtr.Get()))
		{
			if (WaterZone->IsLocalOnlyTessellationEnabled())
			{
				const FBox2D* ZoneUpdateBounds = UpdateBounds.Find(WaterZone);
				if (ZoneUpdateBounds == nullptr || !ZoneUpdateBounds->IsInside(FVector2D(ViewLocation)))
				{
					WaterZone->MarkForRebuild(EWaterZoneRebuildFlags::UpdateWaterInfoTexture);
				}
			}
		}

		bUpdatingWaterInfo = false;
	}

}

void FWaterViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
}

void FWaterViewExtension::MarkWaterInfoTextureForRebuild(const UE::WaterInfo::FRenderingContext& RenderContext)
{
	WaterInfoContextsToRender.Emplace(RenderContext.ZoneToRender, RenderContext);
}

