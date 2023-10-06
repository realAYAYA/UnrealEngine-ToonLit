// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Modules/ModuleInterface.h"

class FRawStatsMemoryProfiler;
class ISessionManager;
class SWidget;

/**
 * Interface for the profiler module.
 */
class IProfilerModule
	: public IModuleInterface
{
public:

	/**
	 * Creates the main window for the profiler.
	 *
	 * @param InSessionManager The session manager to use.
	 * @param ConstructUnderMajorTab The tab which will contain the profiler tabs.
	 *
	 */
	virtual TSharedRef<SWidget> CreateProfilerWindow(const TSharedRef<ISessionManager>& InSessionManager, const TSharedRef<SDockTab>& ConstructUnderMajorTab) = 0;

	/** Implements stats memory dump command. */
	UE_DEPRECATED(5.3, "Use Trace/MemoryInsights and/or LLM for memory profiling.")
	virtual void StatsMemoryDumpCommand(const TCHAR* Filename) = 0;

	/** 
	 * Creates a new instance of the memory profiler based the raw stats. 
	 * When no longer needed must be stopped via RequestStop() and deleted to avoid memory leaks.
	 */
	UE_DEPRECATED(5.3, "Use Trace/MemoryInsights and/or LLM for memory profiling.")
	virtual FRawStatsMemoryProfiler* OpenRawStatsForMemoryProfiling(const TCHAR* Filename) = 0;
};
