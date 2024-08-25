// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AssertionMacros.h"
#include "ProfilingDebugging/ExternalProfiler.h"
#include "Features/IModularFeatures.h"
#include "Templates/UniquePtr.h"
#include "Containers/Map.h"
#include "HAL/ThreadSingleton.h"

#if UE_EXTERNAL_PROFILING_ENABLED && WITH_CONCURRENCYVIEWER_PROFILER

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"
#include <cvmarkers.h>
#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

/**
 * ConcurrencyViewer implementation of FExternalProfiler
 */
class FConcurrencyViewerExternalProfiler : public FExternalProfiler
{
public:

	/** Constructor */
	FConcurrencyViewerExternalProfiler()
	{
		// Register as a modular feature
		IModularFeatures::Get().RegisterModularFeature(FExternalProfiler::GetFeatureName(), this);
	}


	/** Destructor */
	virtual ~FConcurrencyViewerExternalProfiler()
	{
		IModularFeatures::Get().UnregisterModularFeature(FExternalProfiler::GetFeatureName(), this);
	}

	virtual void FrameSync() override
	{
	}

	/** Gets the name of this profiler as a string.  This is used to allow the user to select this profiler in a system configuration file or on the command-line */
	virtual const TCHAR* GetProfilerName() const override
	{
		return TEXT("ConcurrencyViewer");
	}


	/** Pauses profiling. */
	virtual void ProfilerPauseFunction() override
	{
	}

	/** Resumes profiling. */
	virtual void ProfilerResumeFunction() override
	{
	}

	void StartScopedEvent(const struct FColor& Color, const TCHAR* Text) override
	{
		SpanStack.AddZeroed();
		if (MarkerSeries.Num() > 0 && SpanStack.Num() <= MaxDepth)
		{
			CvEnterSpanW(MarkerSeries[SpanStack.Num() - 1], &SpanStack.Last(), Text);
		}
	}

	void StartScopedEvent(const struct FColor& Color, const ANSICHAR* Text) override
	{
		SpanStack.AddZeroed();
		if (MarkerSeries.Num() > 0 && SpanStack.Num() <= MaxDepth)
		{
			CvEnterSpanA(MarkerSeries[SpanStack.Num() - 1], &SpanStack.Last(), Text);
		}
	}

	void EndScopedEvent() override
	{
		if (SpanStack.Num())
		{
			if (SpanStack.Last())
			{
				CvLeaveSpan(SpanStack.Last());
			}
			SpanStack.Pop(EAllowShrinking::No);
		}
	}

	virtual void SetThreadName(const TCHAR* Name) override
	{
		
	}

	/**
	 * Initializes profiler hooks. It is not valid to call pause/ resume on an uninitialized
	 * profiler and the profiler implementation is free to assert or have other undefined
	 * behavior.
	 *
	 * @return true if successful, false otherwise.
	 */
	bool Initialize()
	{
		if (FAILED(CvInitProvider(&CvDefaultProviderGuid, &Provider)))
		{
			return false;
		}
		
		MarkerSeries.SetNum(MaxDepth);
		for (int32 Depth = 0; Depth < MaxDepth; ++Depth)
		{
			// We use 0 left padding so we get proper track sorting in Concurrency Viewer
			if (FAILED(CvCreateMarkerSeries(Provider, *FString::Printf(TEXT("%02d"), Depth), &MarkerSeries[Depth])))
			{
				return false;
			}
		}

		return true;
	}


private:
	const int32 MaxDepth = 99;
	// Those are immutable once initialized
	PCV_PROVIDER Provider = nullptr;
	TArray<PCV_MARKERSERIES> MarkerSeries;

	// Modified during usage
	static thread_local TArray<PCV_SPAN> SpanStack;
};

thread_local TArray<PCV_SPAN> FConcurrencyViewerExternalProfiler::SpanStack;

namespace ConcurrencyViewerProfiler
{
	struct FAtModuleInit
	{
		FAtModuleInit()
		{
			static TUniquePtr<FConcurrencyViewerExternalProfiler> Profiler = MakeUnique<FConcurrencyViewerExternalProfiler>();
			if (!Profiler->Initialize())
			{
				Profiler.Reset();
			}
		}
	};

	static FAtModuleInit AtModuleInit;
}


#endif	// UE_EXTERNAL_PROFILING_ENABLED && WITH_CONCURRENCYVIEWER_PROFILER
