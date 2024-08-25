// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
//#include "NaniteCullRaster.h"

struct FNaniteVisibilityQuery;

class FNaniteVisibilityResults
{
	friend class FNaniteVisibility;
	friend struct FNaniteVisibilityQuery;

public:
	FNaniteVisibilityResults() = default;

	bool IsRasterBinVisible(uint16 BinIndex) const;
	bool IsShadingBinVisible(uint16 BinIndex) const;
	bool IsShadingDrawVisible(uint32 DrawId) const;

	FORCEINLINE bool IsRasterTestValid() const
	{
		return bRasterTestValid;
	}

	FORCEINLINE bool IsShadingTestValid() const
	{
		return bShadingTestValid;
	}

	FORCEINLINE void GetRasterBinStats(uint32& OutNumVisible, uint32& OutNumTotal) const
	{
		OutNumTotal = TotalRasterBins;
		OutNumVisible = IsRasterTestValid() ? VisibleRasterBins : OutNumTotal;
	}

	FORCEINLINE void GetShadingBinStats(uint32& OutNumVisible, uint32& OutNumTotal) const
	{
		OutNumTotal = TotalShadingBins;
		OutNumVisible = IsShadingTestValid() ? VisibleShadingBins : OutNumTotal;
	}

	FORCEINLINE void GetShadingDrawStats(uint32& OutNumVisible, uint32& OutNumTotal) const
	{
		OutNumTotal = TotalShadingDraws;
		OutNumVisible = IsShadingTestValid() ? VisibleShadingDraws : OutNumTotal;
	}

	FORCEINLINE void SetRasterBinIndexTranslator(const FNaniteRasterBinIndexTranslator InTranslator)
	{
		BinIndexTranslator = InTranslator;
	}

	FORCEINLINE bool ShouldRenderCustomDepthPrimitive(uint32 PrimitiveId) const
	{
		if (!bRasterTestValid && !bShadingTestValid)
		{
			// no valid test results, so we didn't visibility test any primitives
			return true;
		}
		return VisibleCustomDepthPrimitives.Contains(PrimitiveId);
	}

	FORCEINLINE const TBitArray<>& GetRasterBinVisibility() const
	{
		return RasterBinVisibility;
	}

	FORCEINLINE const TBitArray<>& GetShadingBinVisibility() const
	{
		return ShadingBinVisibility;
	}

private:
	TBitArray<> RasterBinVisibility;
	TBitArray<> ShadingBinVisibility;
	TArray<uint32, SceneRenderingAllocator> ShadingDrawVisibility;
	TSet<uint32, DefaultKeyFuncs<uint32>, SceneRenderingSetAllocator> VisibleCustomDepthPrimitives;
	FNaniteRasterBinIndexTranslator BinIndexTranslator;
	uint32 TotalRasterBins		= 0;
	uint32 TotalShadingBins		= 0;
	uint32 TotalShadingDraws	= 0;
	uint32 VisibleRasterBins	= 0;
	uint32 VisibleShadingBins	= 0;
	uint32 VisibleShadingDraws	= 0;
	bool bRasterTestValid		= false;
	bool bShadingTestValid		= false;
};

class FNaniteVisibility
{
	friend class FNaniteVisibilityTask;

public:
	struct FRasterBin
	{
		uint16 Primary = 0xFFFFu;
		uint16 Secondary = 0xFFFFu;
	};

	struct FShadingBin
	{
		uint16 Primary = 0xFFFFu;
	};

	using PrimitiveRasterBinType   = TArray<FRasterBin, TInlineAllocator<1>>;
	using PrimitiveShadingBinType  = TArray<FShadingBin, TInlineAllocator<1>>;
	using PrimitiveShadingDrawType = TArray<uint32, TInlineAllocator<1>>;

	struct FPrimitiveReferences
	{
		const FPrimitiveSceneInfo* SceneInfo = nullptr;
		PrimitiveRasterBinType   RasterBins;
		PrimitiveShadingBinType  ShadingBins;
		PrimitiveShadingDrawType ShadingDraws;
		bool bWritesCustomDepthStencil = false;
	};

	using PrimitiveMapType = Experimental::TRobinHoodHashMap<const FPrimitiveSceneInfo*, FPrimitiveReferences>;

public:
	FNaniteVisibility();

	void BeginVisibilityFrame();
	void FinishVisibilityFrame();

	/**
	 * BeginVisibilityQuery and FinishVisibilityQuery are thread safe with respect to each other,
	 * but not with respect to BeginVisibilityFrame/FinishVisibilityFrame.
	 **/
	FNaniteVisibilityQuery* BeginVisibilityQuery(
		FSceneRenderingBulkObjectAllocator& Allocator,
		FScene& Scene,
		const TConstArrayView<FConvexVolume>& ViewList,
		const class FNaniteRasterPipelines* RasterPipelines,
		const class FNaniteShadingPipelines* ShadingPipelines,
		const class FNaniteMaterialCommands* MaterialCommands = nullptr,
		const UE::Tasks::FTask& PrerequisiteTask = {}
	);

	PrimitiveRasterBinType*   GetRasterBinReferences(const FPrimitiveSceneInfo* SceneInfo);
	PrimitiveShadingBinType*  GetShadingBinReferences(const FPrimitiveSceneInfo* SceneInfo);
	PrimitiveShadingDrawType* GetShadingDrawReferences(const FPrimitiveSceneInfo* SceneInfo);

	void RemoveReferences(const FPrimitiveSceneInfo* SceneInfo);

private:
	FPrimitiveReferences* FindOrAddPrimitiveReferences(const FPrimitiveSceneInfo* SceneInfo);

	// Translator should remain valid between Begin/FinishVisibilityFrame. That is, no adding or removing raster bins
	FNaniteRasterBinIndexTranslator BinIndexTranslator;
	TArray<FNaniteVisibilityQuery*, TInlineAllocator<32>> VisibilityQueries;
	TArray<UE::Tasks::FTask, SceneRenderingAllocator> ActiveEvents;
	PrimitiveMapType PrimitiveReferences;
	UE::FMutex Mutex;
	uint8 bCalledBegin : 1;
};

namespace Nanite
{
	extern const FNaniteVisibilityResults* GetVisibilityResults(const FNaniteVisibilityQuery* Query);

	extern UE::Tasks::FTask GetVisibilityTask(const FNaniteVisibilityQuery* Query);
}

class FNaniteScopedVisibilityFrame
{
public:
	FNaniteScopedVisibilityFrame(const bool bInEnabled, FNaniteVisibility& InVisibility)
	: Visibility(InVisibility)
	, bEnabled(bInEnabled)
	{
		if (bEnabled)
		{
			Visibility.BeginVisibilityFrame();
		}
	}

	~FNaniteScopedVisibilityFrame()
	{
		if (bEnabled)
		{
			Visibility.FinishVisibilityFrame();
		}
	}

	FORCEINLINE FNaniteVisibility& Get()
	{
		return Visibility;
	}

private:
	FNaniteVisibility& Visibility;
	bool bEnabled;
};