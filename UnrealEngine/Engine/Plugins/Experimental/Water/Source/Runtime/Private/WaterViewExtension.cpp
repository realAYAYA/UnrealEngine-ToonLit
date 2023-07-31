// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterViewExtension.h"
#include "WaterBodyComponent.h"
#include "WaterSubsystem.h"
#include "WaterModule.h"
#include "WaterZoneActor.h"
#include "WaterInfoRendering.h"
#include "EngineUtils.h"
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

	// #todo_water: hacky way to prevent updating from within the water info passes. Alternative solution might be to set a flag from within the info render
	// similarly to the SetWithinWaterInfoPass functions for water body components 
	//This won't be necessary when waterinfo passes don't utilize scene renderers.
	static bool bUpdatingWaterInfo = false;
	if (!bUpdatingWaterInfo)
	{
		bUpdatingWaterInfo = true;

		for (const TPair<AWaterZone*, UE::WaterInfo::FRenderingContext>& Pair : WaterInfoContextsToRender)
		{
			AWaterZone* WaterZone = Pair.Key;

			if (WaterZone->IsNonTessellatedLODMeshEnabled())
			{
				const UWaterMeshComponent* WaterMesh= WaterZone->GetWaterMeshComponent();
				check(WaterMesh);

				const float SectionSize = WaterZone->GetNonTessellatedLODSectionSize();

				FVector TessellatedWaterMeshCenter = ViewLocation.GridSnap(SectionSize);

				WaterZone->SetTessellatedWaterMeshCenter(TessellatedWaterMeshCenter);

				const FVector TessellatedWaterMeshHalfExtent = WaterZone->GetTessellatedWaterMeshExtent() / 2.0;
				const FBox2D TessellatedWaterMeshBounds(FVector2D(TessellatedWaterMeshCenter - TessellatedWaterMeshHalfExtent), FVector2D(TessellatedWaterMeshCenter + TessellatedWaterMeshHalfExtent));

				WaterZone->GetWaterMeshComponent()->PushTessellatedWaterMeshBoundsToPoxy(TessellatedWaterMeshBounds);

				WaterZone->ForEachWaterBodyComponent([TessellatedWaterMeshBounds](UWaterBodyComponent* WaterBodyComponent)
				{
					WaterBodyComponent->PushTessellatedWaterMeshBoundsToProxy(TessellatedWaterMeshBounds);

					return true;
				});

				const FBox2D CurrentCellBounds(FVector2D(ViewLocation) - FVector2D(SectionSize / 2.0), FVector2D(ViewLocation) + FVector2D(SectionSize / 2.0));
				UpdateBounds.Add(WaterZone, CurrentCellBounds);
			}

			const UE::WaterInfo::FRenderingContext& Context(Pair.Value);
			UE::WaterInfo::UpdateWaterInfoRendering(WorldPtr.Get()->Scene, Context);
		}

		WaterInfoContextsToRender.Empty();

		for (AWaterZone* WaterZone : TActorRange<AWaterZone>(WorldPtr.Get()))
		{
			if (WaterZone->IsNonTessellatedLODMeshEnabled())
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

