// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
VirtualShadowMapClipmap.cpp
=============================================================================*/

#include "VirtualShadowMapClipmap.h"
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RendererModule.h"
#include "VirtualShadowMapArray.h"
#include "VirtualShadowMapCacheManager.h"
#include "VirtualShadowMapDefinitions.h"

extern int32 GForceInvalidateDirectionalVSM;

static TAutoConsoleVariable<float> CVarVirtualShadowMapResolutionLodBiasDirectional(
	TEXT( "r.Shadow.Virtual.ResolutionLodBiasDirectional" ),
	-0.5f,
	TEXT( "Bias applied to LOD calculations for directional lights. -1.0 doubles resolution, 1.0 halves it and so on." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVirtualShadowMapClipmapFirstLevel(
	TEXT( "r.Shadow.Virtual.Clipmap.FirstLevel" ),
	6,
	TEXT( "First level of the virtual clipmap. Lower values allow higher resolution shadows closer to the camera." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVirtualShadowMapClipmapLastLevel(
	TEXT( "r.Shadow.Virtual.Clipmap.LastLevel" ),
	22,
	TEXT( "Last level of the virtual climap. Indirectly determines radius the clipmap can cover." ),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarVirtualShadowMapClipmapFirstCoarseLevel(
	TEXT("r.Shadow.Virtual.Clipmap.FirstCoarseLevel"),
	15,
	TEXT("First level of the clipmap to mark coarse pages for. Lower values allow higher resolution coarse pages near the camera but increase total page counts."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarVirtualShadowMapClipmapLastCoarseLevel(
	TEXT("r.Shadow.Virtual.Clipmap.LastCoarseLevel"),
	18,
	TEXT("Last level of the clipmap to mark coarse pages for. Higher values provide dense clipmap data for a longer radius but increase total page counts."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarVirtualShadowMapClipmapZRangeScale(
	TEXT("r.Shadow.Virtual.Clipmap.ZRangeScale"),
	1000.0f,
	TEXT("Scale of the clipmap level depth range relative to the radius. Should generally be at least 10 or it will result in excessive cache invalidations."),
	ECVF_RenderThreadSafe
);

// "Virtual" clipmap level to clipmap radius
// NOTE: This is the radius of around the clipmap origin that this level must cover
// The actual clipmap dimensions will be larger due to snapping and other accomodations
static float GetLevelRadius(int32 Level)
{
	// NOTE: Virtual clipmap indices can be negative (although not commonly)
	// Clipmap level rounds *down*, so radius needs to cover out to 2^(Level+1), where it flips
	return FMath::Pow(2.0f, static_cast<float>(Level + 1));
}

FVirtualShadowMapClipmap::FVirtualShadowMapClipmap(
	FVirtualShadowMapArray& VirtualShadowMapArray,
	const FLightSceneInfo& InLightSceneInfo,
	const FMatrix& WorldToLightRotationMatrix,
	const FViewMatrices& CameraViewMatrices,
	FIntPoint CameraViewRectSize,
	const FViewInfo* InDependentView)
	: LightSceneInfo(InLightSceneInfo),
	  DependentView(InDependentView)
{
	check(WorldToLightRotationMatrix.GetOrigin() == FVector(0, 0, 0));	// Should not contain translation or scaling

	FVirtualShadowMapArrayCacheManager* VirtualShadowMapArrayCacheManager = VirtualShadowMapArray.CacheManager;

	const bool bCacheValid = VirtualShadowMapArrayCacheManager && VirtualShadowMapArrayCacheManager->IsValid();

	const FMatrix FaceMatrix(
		FPlane( 0, 0, 1, 0 ),
		FPlane( 0, 1, 0, 0 ),
		FPlane(-1, 0, 0, 0 ),
		FPlane( 0, 0, 0, 1 ));

	WorldToLightViewRotationMatrix = WorldToLightRotationMatrix * FaceMatrix;
	// Pure rotation matrix
	FMatrix ViewToWorldRotationMatrix = WorldToLightViewRotationMatrix.GetTransposed();
	
	// NOTE: Rotational (roll) invariance of the directional light depends on square pixels so we just base everything on the camera X scales/resolution
	// NOTE: 0.5 because we double the size of the clipmap region below to handle snapping
	float LodScale = 0.5f / CameraViewMatrices.GetProjectionScale().X;
	LodScale *= float(FVirtualShadowMap::VirtualMaxResolutionXY) / float(CameraViewRectSize.X);
	
	// For now we adjust resolution by just biasing the page we look up in. This is wasteful in terms of page table vs.
	// just resizing the virtual shadow maps for each clipmap, but convenient for now. This means we need to additionally bias
	// which levels are present.
	ResolutionLodBias = CVarVirtualShadowMapResolutionLodBiasDirectional.GetValueOnRenderThread() + FMath::Log2(LodScale);
	// Clamp negative absolute resolution biases as they would exceed the maximum resolution/ranges allocated
	ResolutionLodBias = FMath::Max(0.0f, ResolutionLodBias);

	FirstLevel = CVarVirtualShadowMapClipmapFirstLevel.GetValueOnRenderThread();
	int32 LastLevel = CVarVirtualShadowMapClipmapLastLevel.GetValueOnRenderThread();
	LastLevel = FMath::Max(FirstLevel, LastLevel);
	int32 LevelCount = LastLevel - FirstLevel + 1;

	// Per-clipmap projection data
	LevelData.Empty();
	LevelData.AddDefaulted(LevelCount);

	WorldOrigin = CameraViewMatrices.GetViewOrigin();

	if (InDependentView && InDependentView->ViewState)	// scene capture views don't have a persistent view state
	{
		PerLightCacheEntry = VirtualShadowMapArrayCacheManager->FindCreateLightCacheEntry(LightSceneInfo.Id, InDependentView->ViewState->GetViewKey());
		if (PerLightCacheEntry.IsValid())
		{
			PerLightCacheEntry->UpdateClipmap();
		}
	}

	const int RadiusLn = GetLevelRadius(LastLevel);
	const int RadiiPerLevel = 4;
	FVector SnappedOriginLn = WorldToLightViewRotationMatrix.TransformPosition(WorldOrigin);
	{
		FInt64Point OriginSnapUnitsLn(
			FMath::RoundToInt64(SnappedOriginLn.X / RadiusLn),
			FMath::RoundToInt64(SnappedOriginLn.Y / RadiusLn));
		SnappedOriginLn.X = OriginSnapUnitsLn.X * RadiusLn;
		SnappedOriginLn.Y = OriginSnapUnitsLn.Y * RadiusLn;
	}

	for (int32 Index = 0; Index < LevelCount; ++Index)
	{
		FLevelData& Level = LevelData[Index];
		const int32 AbsoluteLevel = Index + FirstLevel;		// Absolute (virtual) level index

		// TODO: Allocate these as a chunk if we continue to use one per clipmap level
		Level.VirtualShadowMap = VirtualShadowMapArray.Allocate(false);
		ensure(Index == 0 || (Level.VirtualShadowMap->ID == (LevelData[Index-1].VirtualShadowMap->ID + 1)));

		const float RawLevelRadius = GetLevelRadius(AbsoluteLevel);

		double HalfLevelDim = 2.0 * RawLevelRadius;
		double SnapSize = RawLevelRadius;

		FVector ViewCenter = WorldToLightViewRotationMatrix.TransformPosition(WorldOrigin);
		FVector2D CenterSnapUnits(
			FMath::RoundToDouble(ViewCenter.X / SnapSize),
			FMath::RoundToDouble(ViewCenter.Y / SnapSize));
		ViewCenter.X = CenterSnapUnits.X * SnapSize;
		ViewCenter.Y = CenterSnapUnits.Y * SnapSize;

		FInt64Point CornerOffset;
		CornerOffset.X = -(int64_t)CenterSnapUnits.X + (RadiiPerLevel/2);
		CornerOffset.Y =  (int64_t)CenterSnapUnits.Y + (RadiiPerLevel/2);

		const FVector SnappedWorldCenter = ViewToWorldRotationMatrix.TransformPosition(ViewCenter);

		Level.WorldCenter = SnappedWorldCenter;
		Level.CornerOffset = CornerOffset;

		// A relative corner offset is used for LWC reasons.
		// The reference point is WorldOrigin snapped to a grid of GetLevelRadius(LastLevel),
		// because points on this grid are guaranteed to also be present on lower levels,
		// therefore allowing the offsets to represented as factors of level radii without precision loss.
		const FInt64Point SnappedPageOriginLi(-ViewCenter.X, ViewCenter.Y);
		const FInt64Point SnappedPageOriginLn(-SnappedOriginLn.X, SnappedOriginLn.Y);
		const FInt64Point RelativeCornerOffset = SnappedPageOriginLi - SnappedPageOriginLn + ((RadiiPerLevel / 2) * (int64_t)SnapSize);
		Level.RelativeCornerOffset = FIntPoint(RelativeCornerOffset / SnapSize);

		// Check if we have a cache entry for this level
		// If we do and it covers our required depth range, we can use cached pages. Otherwise we need to invalidate.
		TSharedPtr<FVirtualShadowMapCacheEntry> CacheEntry = nullptr;
		if (PerLightCacheEntry.IsValid())
		{
			// NOTE: We use the absolute (virtual) level index so that the caching is robust against changes to the chosen level range
			CacheEntry = PerLightCacheEntry->FindCreateShadowMapEntry(AbsoluteLevel);
		}

		// We expand the depth range of the clipmap level to allow for camera movement without having to invalidate cached shadow data
		// (See VirtualShadowMapCacheManager::UpdateClipmap for invalidation logic.)
		// This also better accomodates SMRT where we want to avoid stepping outside of the Z bounds of a given clipmap
		// NOTE: It's tempting to use a single global Z range for the entire clipmap (which avoids some SMRT overhead too)
		// but this can cause precision issues with cached pages very near the camera.
		const double ViewRadiusZScale = CVarVirtualShadowMapClipmapZRangeScale.GetValueOnRenderThread();

		double ViewRadiusZ = RawLevelRadius * ViewRadiusZScale;
		double ViewCenterDeltaZ = 0.0f;

		if (CacheEntry)
		{
			// We snap to half the size of the VSM at each level
			check((FVirtualShadowMap::Level0DimPagesXY & 1) == 0);
			FInt64Point PageOffset(CornerOffset * (FVirtualShadowMap::Level0DimPagesXY >> 2));

			CacheEntry->UpdateClipmap(Level.VirtualShadowMap->ID,
				WorldToLightRotationMatrix,
				PageOffset,
				CornerOffset,
				RawLevelRadius,
				ViewCenter.Z,
				ViewRadiusZ,
				*PerLightCacheEntry);

			Level.VirtualShadowMap->VirtualShadowMapCacheEntry = CacheEntry;

			// Update min/max Z based on the cached page (if present and valid)
			// We need to ensure we use a consistent depth range as the camera moves for each level
			ViewCenterDeltaZ = ViewCenter.Z - CacheEntry->Clipmap.ViewCenterZ;
			ViewRadiusZ = CacheEntry->Clipmap.ViewRadiusZ;
		}

		// NOTE: These values are all in regular ranges after being offset
		const double ZScale = 0.5 / ViewRadiusZ;
		const double ZOffset = ViewRadiusZ + ViewCenterDeltaZ;
		Level.ViewToClip = FReversedZOrthoMatrix(HalfLevelDim, HalfLevelDim, ZScale, ZOffset);
	}

	ComputeBoundingVolumes(CameraViewMatrices);
}

void FVirtualShadowMapClipmap::ComputeBoundingVolumes(const FViewMatrices& CameraViewMatrices)
{
	// We don't really do much CPU culling with clipmaps. After various testing the fact that we are culling
	// a single frustum that goes out and basically the entire map, and we have to extrude towards (and away!) from
	// the light, and dilate to cover full pages at every clipmap level (to avoid culling something that will go
	// into a page that then gets cached with incomplete geometry), in many situations there is effectively no
	// culling that happens. For instance, as soon as the camera looks vaguely towards or away from the light direction,
	// the extruded frustum effectively covers the whole world.

	const FVector CameraOrigin = CameraViewMatrices.GetViewOrigin();
	const FVector CameraDirection = CameraViewMatrices.GetViewMatrix().GetColumn(2);

	// Thus we don't spend a lot of time trying to optimize for the easy cases and instead just pick an extremely
	// conservative frustum.
	ViewFrustumBounds = FConvexVolume();
	BoundingSphere = FSphere(CameraOrigin, GetMaxRadius());
}

float FVirtualShadowMapClipmap::GetMaxRadius() const
{
	return GetLevelRadius(GetClipmapLevel(GetLevelCount() - 1));
}

FViewMatrices FVirtualShadowMapClipmap::GetViewMatrices(int32 ClipmapIndex) const
{
	check(ClipmapIndex >= 0 && ClipmapIndex < LevelData.Num());
	const FLevelData& Level = LevelData[ClipmapIndex];

	FViewMatrices::FMinimalInitializer Initializer;

	// NOTE: Be careful here! There's special logic in FViewMatrices around ViewOrigin for ortho projections we need to bypass...
	// There's also the fact that some of this data is going to be "wrong", due to the "overridden" matrix thing that shadows do
	Initializer.ViewOrigin = Level.WorldCenter;
	Initializer.ViewRotationMatrix = WorldToLightViewRotationMatrix;
	Initializer.ProjectionMatrix = Level.ViewToClip;

	// TODO: This is probably unused in the shadows/nanite path, but coupling here is not ideal
	Initializer.ConstrainedViewRect = FIntRect(0, 0, FVirtualShadowMap::VirtualMaxResolutionXY, FVirtualShadowMap::VirtualMaxResolutionXY);

	return FViewMatrices(Initializer);
}

FVirtualShadowMapProjectionShaderData FVirtualShadowMapClipmap::GetProjectionShaderData(int32 ClipmapIndex) const
{
	check(ClipmapIndex >= 0 && ClipmapIndex < LevelData.Num());
	const FLevelData& Level = LevelData[ClipmapIndex];
	
	const FLargeWorldRenderPosition PreViewTranslation(GetPreViewTranslation(ClipmapIndex));

	// WorldOrigin should be near the Level.WorldCenter, so we share the LWC tile offset
	// NOTE: We need to negate so that it's not opposite though
	const FVector TileOffset = PreViewTranslation.GetTileOffset();
	const FVector3f NegativeClipmapWorldOriginOffset(-WorldOrigin - TileOffset);

	// NOTE: Some shader logic (projection, etc) assumes some of these parameters are constant across all levels in a clipmap
	FVirtualShadowMapProjectionShaderData Data;
	Data.TranslatedWorldToShadowViewMatrix = FMatrix44f(WorldToLightViewRotationMatrix);
	Data.ShadowViewToClipMatrix = FMatrix44f(Level.ViewToClip);
	Data.TranslatedWorldToShadowUVMatrix = FMatrix44f(CalcTranslatedWorldToShadowUVMatrix(WorldToLightViewRotationMatrix, Level.ViewToClip));
	Data.TranslatedWorldToShadowUVNormalMatrix = FMatrix44f(CalcTranslatedWorldToShadowUVNormalMatrix(WorldToLightViewRotationMatrix, Level.ViewToClip));
	Data.PreViewTranslationLWCTile = PreViewTranslation.GetTile();
	Data.PreViewTranslationLWCOffset = PreViewTranslation.GetOffset();
	Data.LightType = ELightComponentType::LightType_Directional;
	Data.NegativeClipmapWorldOriginLWCOffset = NegativeClipmapWorldOriginOffset;
	Data.ClipmapIndex = ClipmapIndex;
	Data.ClipmapLevel = FirstLevel + ClipmapIndex;
	Data.ClipmapLevelCount = LevelData.Num();
	Data.ResolutionLodBias = ResolutionLodBias;
	Data.ClipmapCornerRelativeOffset = Level.RelativeCornerOffset;
	Data.LightSourceRadius = GetLightSceneInfo().Proxy->GetSourceRadius();
	Data.Flags = GForceInvalidateDirectionalVSM ? VSM_PROJ_FLAG_UNCACHED : 0U;

	return Data;
}

uint32 FVirtualShadowMapClipmap::GetCoarsePageClipmapIndexMask()
{
	uint32 BitMask = 0;

	const int FirstLevel = CVarVirtualShadowMapClipmapFirstLevel.GetValueOnRenderThread();
	const int LastLevel  = FMath::Max(FirstLevel, CVarVirtualShadowMapClipmapLastLevel.GetValueOnRenderThread());	
	int FirstCoarseIndex = CVarVirtualShadowMapClipmapFirstCoarseLevel.GetValueOnRenderThread() - FirstLevel;
	int LastCoarseIndex  = CVarVirtualShadowMapClipmapLastCoarseLevel.GetValueOnRenderThread() - FirstLevel;	

	ensureMsgf((LastLevel - FirstLevel) < 32, TEXT("Too many clipmap levels for coarse page bitmask."));

	FirstCoarseIndex = FMath::Max(0, FirstCoarseIndex);
	if (LastCoarseIndex >= FirstCoarseIndex)
	{
		uint32 BitCount = static_cast<uint32>(LastCoarseIndex - FirstCoarseIndex + 1);
		uint32 BitRange = (1 << BitCount) - 1;
		BitMask = BitMask | (BitRange << FirstCoarseIndex);
	}

	// Always mark coarse pages in the last level for clouds/skyatmosphere
	BitMask = BitMask | (1 << (LastLevel - FirstLevel));

	return BitMask;
}


static inline void LazyInitAndSetBitArray(TBitArray<>& BitArray, int32 Index, bool Value, int32 MaxNum)
{
	if (BitArray.IsEmpty())
	{
		BitArray.Init(false, MaxNum);
	}
	BitArray[Index] = Value;

}

void FVirtualShadowMapClipmap::OnPrimitiveRendered(const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	if (PerLightCacheEntry.IsValid())
	{
		FPersistentPrimitiveIndex PersistentPrimitiveId = PrimitiveSceneInfo->GetPersistentIndex();
		check(PersistentPrimitiveId.Index >= 0);
		check(PersistentPrimitiveId.Index < PerLightCacheEntry->RenderedPrimitives.Num());

		// Check previous-frame state to detect transition from hidden->visible
		if (!PerLightCacheEntry->RenderedPrimitives[PersistentPrimitiveId.Index])
		{
			LazyInitAndSetBitArray(RevealedPrimitivesMask, PersistentPrimitiveId.Index, true, PerLightCacheEntry->RenderedPrimitives.Num());
		}

		// update current frame-state.
		LazyInitAndSetBitArray(RenderedPrimitives, PersistentPrimitiveId.Index, true, PerLightCacheEntry->RenderedPrimitives.Num());

		// update cached state (this is checked & cleared whenever a primitive is invalidating the VSM).
		PerLightCacheEntry->OnPrimitiveRendered(PrimitiveSceneInfo);
	}
}

void FVirtualShadowMapClipmap::UpdateCachedFrameData()
{
	if (PerLightCacheEntry.IsValid())
	{
		PerLightCacheEntry->RenderedPrimitives = MoveTemp(RenderedPrimitives);
	}
}