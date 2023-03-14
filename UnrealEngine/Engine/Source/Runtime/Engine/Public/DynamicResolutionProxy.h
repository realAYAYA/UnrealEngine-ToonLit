// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneView.h"
#include "Engine/EngineTypes.h"


/** Render thread proxy that hold's the heuristic for dynamic resolution. */
class ENGINE_API FDynamicResolutionHeuristicProxy
{
public:
	static constexpr uint64 kInvalidEntryId = ~uint64(0);


	FDynamicResolutionHeuristicProxy();
	~FDynamicResolutionHeuristicProxy();


	/** Resets the proxy. */
	void Reset_RenderThread();

	/** Create a new previous frame and feeds its timings and returns history's unique id. */
	uint64 CreateNewPreviousFrameTimings_RenderThread(float GameThreadTimeMs, float RenderThreadTimeMs);

	/** Commit GPU busy times */
	void CommitPreviousFrameGPUTimings_RenderThread(
		uint64 HistoryFrameId,
		float TotalFrameGPUBusyTimeMs,
		float DynamicResolutionGPUBusyTimeMs,
		const DynamicRenderScaling::TMap<float>& BudgetTimingMs);

	/** Refresh resolution fraction's from history. */
	void RefreshCurentFrameResolutionFraction_RenderThread();

	/** Returns the view fraction that should be used for current frame. */
	FORCEINLINE DynamicRenderScaling::TMap<float> QueryCurentFrameResolutionFractions() const
	{
		check(IsInRenderingThread());
		return QueryCurentFrameResolutionFractions_Internal();
	}

	/** Returns a non thread safe approximation of the current resolution fraction applied on render thread. */
	FORCEINLINE DynamicRenderScaling::TMap<float> GetResolutionFractionsApproximation_GameThread() const
	{
		check(IsInGameThread());
		return QueryCurentFrameResolutionFractions_Internal();
	}


	/** Returns the view fraction upper bound. */
	DynamicRenderScaling::TMap<float> GetResolutionFractionUpperBounds() const;

	/** Creates a default dynamic resolution state using this proxy that queries GPU timing from the RHI. */
	static TSharedPtr< class IDynamicResolutionState > CreateDefaultState();


private:
	struct FrameHistoryEntry
	{
		// Thread timings in milliseconds.
		float GameThreadTimeMs;
		float RenderThreadTimeMs;

		// Total GPU busy time for the entire frame in milliseconds.
		float TotalFrameGPUBusyTimeMs;

		// Total GPU busy time for the render thread commands that does dynamic resolutions.
		float GlobalDynamicResolutionTimeMs;

		// Time for each individual timings
		DynamicRenderScaling::TMap<float> BudgetTimingMs;

		// The resolution fraction the frame was rendered with.
		DynamicRenderScaling::TMap<float> ResolutionFractions;

		inline FrameHistoryEntry()
			: GameThreadTimeMs(-1.0f)
			, RenderThreadTimeMs(-1.0f)
			, TotalFrameGPUBusyTimeMs(-1.0f)
			, GlobalDynamicResolutionTimeMs(-1.0f)
		{
			ResolutionFractions.SetAll(1.0f);
			BudgetTimingMs.SetAll(-1.0f);
		}


		// Returns whether GPU timings have landed.
		inline bool HasGPUTimings() const
		{
			return TotalFrameGPUBusyTimeMs >= 0.0f;
		}
	};

	// Circular buffer of the history.
	// We don't use TCircularBuffer because does not support resizes.
	TArray<FrameHistoryEntry> History;
	int32 PreviousFrameIndex;
	int32 HistorySize;

	// Counts the number of frame since the last screen percentage change.
	int32 NumberOfFramesSinceScreenPercentageChange;

	// Number of frame remaining to ignore.
	int32 IgnoreFrameRemainingCount;

	// Current frame's view fraction.
	DynamicRenderScaling::TMap<float> CurrentFrameResolutionFractions;
	DynamicRenderScaling::TMap<float> CurrentFrameMaxResolutionFractions;
	DynamicRenderScaling::TMap<int32> BudgetHistorySizes;

	// Frame counter to allocate unique ID for CommitPreviousFrameGPUTimings_RenderThread().
	uint64 FrameCounter;


	inline const FrameHistoryEntry& GetPreviousFrameEntry(int32 BrowsingFrameId) const
	{
		if (BrowsingFrameId < 0 || BrowsingFrameId >= HistorySize)
		{
			static const FrameHistoryEntry InvalidEntry;
			return InvalidEntry;
		}
		return History[(History.Num() + PreviousFrameIndex - BrowsingFrameId) % History.Num()];
	}

	DynamicRenderScaling::TMap<float> QueryCurentFrameResolutionFractions_Internal() const;

	void RefreshCurentFrameResolutionFractionUpperBound_RenderThread();

	void ResetInternal();

	void ResizeHistoryIfNeeded();
};

