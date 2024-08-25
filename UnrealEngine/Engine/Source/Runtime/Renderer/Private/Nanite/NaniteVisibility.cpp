// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteVisibility.h"
#include "NaniteMaterials.h"
#include "NaniteSceneProxy.h"
#include "ScenePrivate.h"
#include "InstanceDataSceneProxy.h"

static int32 GNaniteMaterialVisibility = 0;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibility(
	TEXT("r.Nanite.MaterialVisibility"),
	GNaniteMaterialVisibility,
	TEXT("Whether to enable Nanite material visibility tests"),
	ECVF_ReadOnly
);

static int32 GNaniteMaterialVisibilityAsync = 1;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityAsync(
	TEXT("r.Nanite.MaterialVisibility.Async"),
	GNaniteMaterialVisibilityAsync,
	TEXT("Whether to enable parallelization of Nanite material visibility tests"),
	ECVF_RenderThreadSafe
);

int32 GNaniteMaterialVisibilityPrimitives = 1;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityPrimitives(
	TEXT("r.Nanite.MaterialVisibility.Primitives"),
	GNaniteMaterialVisibilityPrimitives,
	TEXT("")
);

int32 GNaniteMaterialVisibilityInstances = 0;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityInstances(
	TEXT("r.Nanite.MaterialVisibility.Instances"),
	GNaniteMaterialVisibilityInstances,
	TEXT("")
);

int32 GNaniteMaterialVisibilityRasterBins = 1;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityRasterBins(
	TEXT("r.Nanite.MaterialVisibility.RasterBins"),
	GNaniteMaterialVisibilityRasterBins,
	TEXT("")
);

int32 GNaniteMaterialVisibilityShadingBins = 1;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityShadingBins(
	TEXT("r.Nanite.MaterialVisibility.ShadingBins"),
	GNaniteMaterialVisibilityShadingBins,
	TEXT("")
);

struct FNaniteVisibilityQuery
{
	void Init(
		const FNaniteRasterPipelines* RasterPipelines,
		const FNaniteShadingPipelines* ShadingPipelines,
		const FNaniteMaterialCommands* MaterialCommands)
	{
		RasterBinCount = RasterPipelines->GetBinCount();
		ShadingBinCount = ShadingPipelines->GetBinCount();
		ShadingDrawCount = MaterialCommands->GetCommands().Num();
		BinIndexTranslator = RasterPipelines->GetBinIndexTranslator();
		ShadingDrawVisibility.Reserve(ShadingDrawCount);

		RasterBinVisibility.SetNum(RasterBinCount);
		for (uint32 RasterBinIndex = 0; RasterBinIndex < RasterBinCount; ++RasterBinIndex)
		{
			RasterBinVisibility[int32(RasterBinIndex)] = false;
		}

		ShadingBinVisibility.SetNum(ShadingBinCount);
		for (uint32 ShadingBinIndex = 0; ShadingBinIndex < ShadingBinCount; ++ShadingBinIndex)
		{
			ShadingBinVisibility[int32(ShadingBinIndex)] = false;
		}
	}

	void Finish()
	{
		check(!bFinished);
		SCOPED_NAMED_EVENT_TEXT("EndPerformNaniteVisibility", FColor::Magenta);

		Results.SetRasterBinIndexTranslator(BinIndexTranslator);
		Results.bRasterTestValid = bCullRasterBins;
		Results.bShadingTestValid = bCullShadingBins;

		if (Results.bRasterTestValid)
		{
			Results.RasterBinVisibility.Init(false, RasterBinVisibility.Num());
			for (int32 RasterBinIndex = 0; RasterBinIndex < RasterBinVisibility.Num(); ++RasterBinIndex)
			{
				if (RasterBinVisibility[RasterBinIndex])
				{
					Results.RasterBinVisibility[RasterBinIndex] = true;
					++Results.VisibleRasterBins;
				}
			}
		}

		if (Results.bShadingTestValid)
		{
			if (bUseComputeMaterials)
			{
				Results.ShadingBinVisibility.Init(false, ShadingBinVisibility.Num());
				for (int32 ShadingBinIndex = 0; ShadingBinIndex < ShadingBinVisibility.Num(); ++ShadingBinIndex)
				{
					if (ShadingBinVisibility[ShadingBinIndex])
					{
						Results.ShadingBinVisibility[ShadingBinIndex] = true;
						++Results.VisibleShadingBins;
					}
				}

				Results.TotalShadingBins = ShadingBinCount;
			}
			else
			{
				Results.ShadingDrawVisibility = ShadingDrawVisibility.Array();
				Results.VisibleShadingDraws = Results.ShadingDrawVisibility.Num();
				Results.TotalShadingDraws = ShadingDrawCount;
			}
		}

		Results.TotalRasterBins = RasterBinCount;
		Results.VisibleCustomDepthPrimitives = MoveTemp(VisibleCustomDepthPrimitives);
		bFinished = true;

		// The query is complete; release working memory.
		Views.Empty();
		RasterBinVisibility.Empty();
		ShadingBinVisibility.Empty();
		ShadingDrawVisibility.Empty();
		VisibleCustomDepthPrimitives.Empty();
	}

	UE::Tasks::FTask		CompletedEvent;
	TArray<FConvexVolume, SceneRenderingAllocator> Views;
	TArray<TAtomic<bool>, SceneRenderingAllocator> RasterBinVisibility;
	TArray<TAtomic<bool>, SceneRenderingAllocator> ShadingBinVisibility;
	TSet<uint32, DefaultKeyFuncs<uint32>, SceneRenderingSetAllocator> ShadingDrawVisibility;
	TSet<uint32, DefaultKeyFuncs<uint32>, SceneRenderingSetAllocator> VisibleCustomDepthPrimitives;

	FNaniteVisibilityResults Results;

	FNaniteRasterBinIndexTranslator BinIndexTranslator;

	uint32 RasterBinCount;
	uint32 ShadingBinCount;
	uint32 ShadingDrawCount;

	uint8 bFinished				: 1;
	uint8 bCullRasterBins		: 1;
	uint8 bCullShadingBins		: 1;
	uint8 bUseComputeMaterials	: 1;
};

namespace Nanite
{
	const FNaniteVisibilityResults* GetVisibilityResults(const FNaniteVisibilityQuery* Query)
	{
		if (Query)
		{
			Query->CompletedEvent.Wait();
			return &Query->Results;
		}
		return nullptr;
	}

	UE::Tasks::FTask GetVisibilityTask(const FNaniteVisibilityQuery* Query)
	{
		if (Query)
		{
			return Query->CompletedEvent;
		}
		return {};
	}
}

bool FNaniteVisibilityResults::IsRasterBinVisible(uint16 BinIndex) const
{
	return IsRasterTestValid() ? RasterBinVisibility[int32(BinIndexTranslator.Translate(BinIndex))] : true;
}

bool FNaniteVisibilityResults::IsShadingBinVisible(uint16 BinIndex) const
{
	return IsShadingTestValid() ? ShadingBinVisibility[int32(BinIndex)] : true;
}

bool FNaniteVisibilityResults::IsShadingDrawVisible(uint32 DrawId) const
{
	return IsShadingTestValid() ? ShadingDrawVisibility.Contains(DrawId) : true;
}

static FORCEINLINE bool IsVisibilityTestNeeded(
	const FNaniteVisibilityQuery* Query,
	const FNaniteVisibility::FPrimitiveReferences& References,
	const FNaniteRasterBinIndexTranslator BinIndexTranslator,
	bool bAsync)
{
	bool bShouldTest = false;

	for (const FNaniteVisibility::FRasterBin& RasterBin : References.RasterBins)
	{
		const bool bPrimaryVisible = Query->RasterBinVisibility[int32(BinIndexTranslator.Translate(RasterBin.Primary))];
		const bool bSecondaryVisible = RasterBin.Secondary != 0xFFFFu ? (bool)(Query->RasterBinVisibility[int32(BinIndexTranslator.Translate(RasterBin.Secondary))]) : true;

		if (!bPrimaryVisible || !bSecondaryVisible) // Raster bin reference is not marked visible
		{
			bShouldTest = true;
			break;
		}
	}

	if (!bShouldTest)
	{
		if (Query->bUseComputeMaterials)
		{
			for (const FNaniteVisibility::FShadingBin& ShadingBin : References.ShadingBins)
			{
				const bool bPrimaryVisible = Query->ShadingBinVisibility[int32(ShadingBin.Primary)];
				if (!bPrimaryVisible) // Shading bin reference is not marked visible
				{
					bShouldTest = true;
					break;
				}
			}
		}
		else
		{
			for (const uint32& ShadingDrawId : References.ShadingDraws)
			{
				if (!Query->ShadingDrawVisibility.Contains(ShadingDrawId)) // Shading draw reference is not present
				{
					bShouldTest = true;
					break;
				}
			}
		}
	}

	return bShouldTest;
}

static FORCEINLINE bool IsNanitePrimitiveVisible(const FNaniteVisibilityQuery* Query, const FPrimitiveSceneInfo* SceneInfo)
{
	FPrimitiveSceneProxy* SceneProxy = SceneInfo->Proxy;
	if (!SceneProxy || SceneInfo->Scene == nullptr || !SceneInfo->IsIndexValid())
	{
		return false;
	}

	bool bPrimitiveVisible = true;
	
	if (GNaniteMaterialVisibilityPrimitives != 0)
	{
		bPrimitiveVisible = false;

		const FBoxSphereBounds& ProxyBounds = SceneInfo->Scene->PrimitiveBounds[SceneInfo->GetIndex()].BoxSphereBounds; // World space bounds

		for (const FConvexVolume& View : Query->Views)
		{
			bPrimitiveVisible = View.IntersectBox(ProxyBounds.Origin, ProxyBounds.BoxExtent);
			if (bPrimitiveVisible)
			{
				break;
			}
		}
	}

	if (bPrimitiveVisible && GNaniteMaterialVisibilityInstances != 0)
	{
		if (const FInstanceSceneDataBuffers *InstanceData = SceneInfo->GetInstanceSceneDataBuffers())
		{
			bPrimitiveVisible = false;
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceData->GetNumInstances(); ++InstanceIndex)
			{
				const FBoxSphereBounds InstanceWorldBounds = InstanceData->GetInstanceWorldBounds(InstanceIndex);

				for (const FConvexVolume& View : Query->Views)
				{
					bPrimitiveVisible = View.IntersectBox(InstanceWorldBounds.Origin, InstanceWorldBounds.BoxExtent);
					if (bPrimitiveVisible)
					{
						break;
					}
				}

				if (bPrimitiveVisible)
				{
					break;
				}
			}
		}
	}

	return bPrimitiveVisible;
}

static void PerformNaniteVisibility(const FNaniteVisibility::PrimitiveMapType& PrimitiveReferences, FNaniteVisibilityQuery* Query)
{
	SCOPED_NAMED_EVENT(PerformNaniteVisibility, FColor::Magenta);

	if (PrimitiveReferences.Num() == 0)
	{
		return;
	}

	for (const auto& KeyValue : PrimitiveReferences)
	{
		const FNaniteVisibility::FPrimitiveReferences& References = KeyValue.Value;

		bool bPrimitiveVisible = true;
		const bool bShouldTest = IsVisibilityTestNeeded(Query, References, Query->BinIndexTranslator, false /* Async */);
		if (bShouldTest)
		{
			bPrimitiveVisible = IsNanitePrimitiveVisible(Query, References.SceneInfo);
			if (bPrimitiveVisible)
			{
				if (Query->bCullRasterBins)
				{
					for (const FNaniteVisibility::FRasterBin& RasterBin : References.RasterBins)
					{
						Query->RasterBinVisibility[int32(Query->BinIndexTranslator.Translate(RasterBin.Primary))] = true;
						if (RasterBin.Secondary != 0xFFFFu)
						{
							Query->RasterBinVisibility[int32(Query->BinIndexTranslator.Translate(RasterBin.Secondary))] = true;
						}
					}
				}

				if (Query->bCullShadingBins)
				{
					if (Query->bUseComputeMaterials)
					{
						for (const FNaniteVisibility::FShadingBin& ShadingBin : References.ShadingBins)
						{
							Query->ShadingBinVisibility[int32(ShadingBin.Primary)] = true;
						}
					}
					else
					{
						Query->ShadingDrawVisibility.Append(References.ShadingDraws);
					}
				}
			}
		}

		// NOTE: This makes the assumption that the visibility test doesn't occlusion cull
		if (Nanite::GetSupportsCustomDepthRendering() && References.bWritesCustomDepthStencil && bPrimitiveVisible)
		{
			Query->VisibleCustomDepthPrimitives.Add(References.SceneInfo->GetIndex());
		}
	}
}

FNaniteVisibility::FNaniteVisibility()
: bCalledBegin(false)
{
}

void FNaniteVisibility::BeginVisibilityFrame()
{
	check(VisibilityQueries.Num() == 0);
	check(!bCalledBegin);
	bCalledBegin = true;
}

void FNaniteVisibility::FinishVisibilityFrame()
{
	check(bCalledBegin);

	if (!ActiveEvents.IsEmpty())
	{
		UE::Tasks::Wait(ActiveEvents);
		ActiveEvents.Empty();
	}

	VisibilityQueries.Empty();
	bCalledBegin = false;
}

FNaniteVisibilityQuery* FNaniteVisibility::BeginVisibilityQuery(
	FSceneRenderingBulkObjectAllocator& Allocator,
	FScene& Scene,
	const TConstArrayView<FConvexVolume>& ViewList,
	const class FNaniteRasterPipelines* RasterPipelines,
	const class FNaniteShadingPipelines* ShadingPipelines,
	const class FNaniteMaterialCommands* MaterialCommands,
	const UE::Tasks::FTask& PrerequisiteTask
)
{
	check(RasterPipelines);
	check(MaterialCommands);

	if (!bCalledBegin || ViewList.IsEmpty() || GNaniteMaterialVisibility == 0)
	{
		// Nothing to do
		return nullptr;
	}

	const bool bRunAsync = GNaniteMaterialVisibilityAsync != 0;
	const bool bUseComputeMaterials = UseNaniteComputeMaterials();

	FNaniteVisibilityQuery* VisibilityQuery = Allocator.Create<FNaniteVisibilityQuery>();
	VisibilityQuery->Views = ViewList;
	VisibilityQuery->bCullRasterBins		= GNaniteMaterialVisibilityRasterBins  != 0;
	VisibilityQuery->bCullShadingBins		= GNaniteMaterialVisibilityShadingBins != 0;
	VisibilityQuery->bUseComputeMaterials	= bUseComputeMaterials;

	VisibilityQuery->bFinished = false;

	const UE::Tasks::EExtendedTaskPriority ExtendedTaskPriority = bRunAsync ? UE::Tasks::EExtendedTaskPriority::None : UE::Tasks::EExtendedTaskPriority::Inline;

		VisibilityQuery->CompletedEvent = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, VisibilityQuery, RasterPipelines, ShadingPipelines, MaterialCommands]
		{
			VisibilityQuery->Init(RasterPipelines, ShadingPipelines, MaterialCommands);
			PerformNaniteVisibility(PrimitiveReferences, VisibilityQuery);
		VisibilityQuery->Finish();

	}, MakeArrayView({ Scene.GetCacheNaniteMaterialBinsTask(), PrerequisiteTask }), UE::Tasks::ETaskPriority::Normal, ExtendedTaskPriority);

	{
		UE::TUniqueLock Lock(Mutex);
		VisibilityQueries.Emplace(VisibilityQuery);
		if (VisibilityQuery->CompletedEvent.IsValid())
		{
			ActiveEvents.Emplace(VisibilityQuery->CompletedEvent);
		}
	}

	return VisibilityQuery;
}

FNaniteVisibility::FPrimitiveReferences* FNaniteVisibility::FindOrAddPrimitiveReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	if (!GNaniteMaterialVisibility)
	{
		return nullptr;
	}

	FNaniteVisibility::FPrimitiveReferences* References = PrimitiveReferences.FindOrAdd(SceneInfo, FNaniteVisibility::FPrimitiveReferences{});

	// If we perform visibility query for either raster bins or shading, we can piggy back the testing to further cull Nanite
	// custom depth instances on the view
	if (SceneInfo->Proxy && SceneInfo->Proxy->ShouldRenderCustomDepth())
	{
		References->bWritesCustomDepthStencil = true;
	}

	return References;
}

FNaniteVisibility::PrimitiveRasterBinType* FNaniteVisibility::GetRasterBinReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	if (!GNaniteMaterialVisibility)
	{
		return nullptr;
	}

	FNaniteVisibility::FPrimitiveReferences* References = FindOrAddPrimitiveReferences(SceneInfo);
	References->SceneInfo = SceneInfo;
	return &References->RasterBins;
}

FNaniteVisibility::PrimitiveShadingBinType* FNaniteVisibility::GetShadingBinReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	if (!GNaniteMaterialVisibility)
	{
		return nullptr;
	}

	FNaniteVisibility::FPrimitiveReferences* References = FindOrAddPrimitiveReferences(SceneInfo);
	References->SceneInfo = SceneInfo;
	return &References->ShadingBins;
}

FNaniteVisibility::PrimitiveShadingDrawType* FNaniteVisibility::GetShadingDrawReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	if (!GNaniteMaterialVisibility)
	{
		return nullptr;
	}

	FNaniteVisibility::FPrimitiveReferences* References = FindOrAddPrimitiveReferences(SceneInfo);
	References->SceneInfo = SceneInfo;
	return &References->ShadingDraws;
}

void FNaniteVisibility::RemoveReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	// Always remove references even when Nanite visibility is disabled, as the CVar could change state while a primitive is attached to the scene.
	PrimitiveReferences.Remove(SceneInfo);
}