// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShadowScene.h"
#include "ScenePrivate.h"
#include "Algo/ForEach.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "DynamicPrimitiveDrawing.h"
#include "LightSceneProxy.h"
#endif

TAutoConsoleVariable<int32> CVarDebugDrawLightActiveStateTracking(
	TEXT("r.Shadow.Scene.DebugDrawLightActiveStateTracking"),
	0,
	TEXT("."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarShadowSceneLightActiveFrameCount(
	TEXT("r.Shadow.Scene.LightActiveFrameCount"),
	10,
	TEXT("Number of frames before a light that has been moving (updated or transform changed) goes to inactive state.\n")
	TEXT("  This determines the number of frames that the MobilityFactor goes to zero over, and thus a higher number spreads invalidations out over a longer time."),
	ECVF_RenderThreadSafe
);

DECLARE_DWORD_COUNTER_STAT(TEXT("Active Light Count"), STAT_ActiveLightCount, STATGROUP_ShadowRendering);


FShadowScene::FShadowScene(FScene & InScene)
	: Scene(InScene)
{
	Scene.OnPostLightSceneInfoUpdate.AddLambda([this](FRDGBuilder& GraphBuilder, const FLightSceneChangeSet& LightSceneChangeSet) { PostLightsUpdate(GraphBuilder, LightSceneChangeSet); });
}

void FShadowScene::PostLightsUpdate(FRDGBuilder& GraphBuilder, const FLightSceneChangeSet& LightSceneChangeSet)
{
	// don't spawn async work for no good reason.
	constexpr int32 kMinWorkSizeForAsync = 64;

	int32 WorkSize = LightSceneChangeSet.AddedLightIds.Num() + LightSceneChangeSet.RemovedLightIds.Num() + LightSceneChangeSet.TransformUpdatedLightIds.Num();

	// Don't sync, or kick off a new job if there is no work to do.
	if (WorkSize > 0)
	{
		// Need to wait in case the update is performed several times in a row for some reason.
		SceneChangeUpdateTask.Wait();
		SceneChangeUpdateTask = GraphBuilder.AddSetupTask([this, LightSceneChangeSet]
		{
			// Oust all removed Ids.
			for (int32 Id : LightSceneChangeSet.RemovedLightIds)
			{
				check(Lights.IsValidIndex(Id));
				ActiveLights[Id] = false;
				Lights.RemoveAt(Id);
			}
	
			// Track active lights (those that are or were moving, and thus need updating)
			ActiveLights.SetNum(FMath::Max(Lights.GetMaxIndex(), Lights.Num() + LightSceneChangeSet.AddedLightIds.Num()), false);

			auto UpdateLight = [&](int32 Id)
			{
				FLightData& Light = GetOrAddLight(Id);
				// Mark as not rendered
				Light.FirstActiveFrameNumber = INDEX_NONE;
				// Only movable lights can become "active" (i.e., having moved recently and thus needing active update)
				if (Scene.Lights[Id].bIsMovable)
				{
					ActiveLights[Id] = true;
					Light.MobilityFactor = 1.0f;
				}
				else
				{
					ActiveLights[Id] = false;
					Light.MobilityFactor = 0.0f;
					// Go straight to not active
				}
			};

			Algo::ForEach(LightSceneChangeSet.AddedLightIds, UpdateLight);
			Algo::ForEach(LightSceneChangeSet.TransformUpdatedLightIds, UpdateLight);

		}, WorkSize > kMinWorkSizeForAsync);
	}
}

void FShadowScene::PostSceneUpdate(const FScenePreUpdateChangeSet &ScenePreUpdateChangeSet, const FScenePostUpdateChangeSet &ScenePostUpdateChangeSet)
{
	for (FPrimitiveSceneInfo* PrimitiveSceneInfo : ScenePreUpdateChangeSet.RemovedPrimitiveSceneInfos)
	{
		if (PrimitiveSceneInfo->Proxy->GetShadowCacheInvalidationBehavior() == EShadowCacheInvalidationBehavior::Always)
		{
			AlwaysInvalidatingPrimitives.RemoveSwap(PrimitiveSceneInfo);
		}
	}
	for (FPrimitiveSceneInfo* PrimitiveSceneInfo : ScenePostUpdateChangeSet.AddedPrimitiveSceneInfos)
	{
		if (PrimitiveSceneInfo->Proxy->GetShadowCacheInvalidationBehavior() == EShadowCacheInvalidationBehavior::Always)
		{
			AlwaysInvalidatingPrimitives.Add(PrimitiveSceneInfo);
		}
	}
}

void FShadowScene::UpdateForRenderedFrame(FRDGBuilder& GraphBuilder)
{
	SceneChangeUpdateTask.Wait();

	const int32 ActiveFrameCount = FMath::Max(1, CVarShadowSceneLightActiveFrameCount.GetValueOnRenderThread());
	
	// 1. FScene::FrameNumber is incremented before a call to render is being dispatched to the RT.
	int32 SceneFrameNumber = Scene.GetFrameNumberRenderThread();
	// Iterate the previously active lights and update
	for (TConstSetBitIterator<> BitIt(ActiveLights); BitIt; ++BitIt)
	{
		int32 Id = BitIt.GetIndex();
		// No need to process if we already decided it is active (i.e., it was moved again this frame)
		FLightData& Light = Lights[Id];
		
		// If it was not rendered before, we record the current scene frame number
		if (Light.FirstActiveFrameNumber == INDEX_NONE)
		{
			Light.FirstActiveFrameNumber = SceneFrameNumber;
			Light.MobilityFactor = 1.0f;
		}
		else if(SceneFrameNumber - Light.FirstActiveFrameNumber  < ActiveFrameCount)
		{
			// If it was updated before, but not this frame, check how many rendered frames have elapsed and transition to the inactive state
			Light.MobilityFactor = 1.0f - FMath::Clamp(float(SceneFrameNumber - Light.FirstActiveFrameNumber) / float(ActiveFrameCount), 0.0f, 1.0f);
		}
		else
		{
			// It's not been updated for more than K frames transition to non-active state
			ActiveLights[Id] = false;
			Light.MobilityFactor = 0.0f;
		}
	}

	SET_DWORD_STAT(STAT_ActiveLightCount, ActiveLights.CountSetBits());
}



float FShadowScene::GetLightMobilityFactor(int32 LightId) const 
{ 
	SceneChangeUpdateTask.Wait();

	if (IsActive(LightId))
	{
		return Lights[LightId].MobilityFactor;
	}
	return 0.0f;
}


void FShadowScene::DebugRender(TArrayView<FViewInfo> Views)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	if (CVarDebugDrawLightActiveStateTracking.GetValueOnRenderThread() != 0)
	{
		SceneChangeUpdateTask.Wait();

		for (FViewInfo& View : Views)
		{
			FViewElementPDI DebugPDI(&View, nullptr, &View.DynamicPrimitiveCollector);

			for (TConstSetBitIterator<> BitIt(ActiveLights); BitIt; ++BitIt)
			{
				int32 Id = BitIt.GetIndex();
				FLightSceneInfoCompact& LightSceneInfoCompact = Scene.Lights[Id];
				FLightSceneProxy* Proxy = LightSceneInfoCompact.LightSceneInfo->Proxy;
				FLinearColor Color = FMath::Lerp(FLinearColor(FColor::Yellow), FLinearColor(FColor::Blue), Scene.ShadowScene->GetLightMobilityFactor(Id));
				switch (LightSceneInfoCompact.LightType)
				{
				case LightType_Directional:
					DrawWireSphereAutoSides(&DebugPDI, Proxy->GetLightToWorld().GetOrigin(), Color, FMath::Min(100.0f, Proxy->GetRadius()), SDPG_World);
					DrawWireSphereAutoSides(&DebugPDI, Proxy->GetLightToWorld().GetOrigin(), Color, FMath::Min(200.0f, Proxy->GetRadius()), SDPG_World);
					break;
				case LightType_Spot:
				{
					FTransform TransformNoScale = FTransform(Proxy->GetLightToWorld());
					TransformNoScale.RemoveScaling();

					DrawWireSphereCappedCone(&DebugPDI, TransformNoScale, Proxy->GetRadius(), FMath::RadiansToDegrees(Proxy->GetOuterConeAngle()), 16, 4, 8, Color, SDPG_World);
				}
				break;
				default:
				{
					DrawWireSphereAutoSides(&DebugPDI, Proxy->GetPosition(), Color, Proxy->GetRadius(), SDPG_World);
				}
				break;
				};
			}
		}
	}

#endif
}