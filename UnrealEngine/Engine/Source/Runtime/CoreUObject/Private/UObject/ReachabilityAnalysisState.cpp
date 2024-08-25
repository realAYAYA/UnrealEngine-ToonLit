// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ReachabilityAnalysisState.cpp: Incremental reachability analysis support.
=============================================================================*/

#include "UObject/ReachabilityAnalysisState.h"
#include "UObject/ReachabilityAnalysis.h"

namespace UE::GC
{

void FReachabilityAnalysisState::Init()
{
	NumIterations = 0;
}

void FReachabilityAnalysisState::SetupWorkers(int32 InNumWorkers)
{
	NumWorkers = InNumWorkers;
	Stats = {};
	bIsSuspended = false;
}

void FReachabilityAnalysisState::UpdateStats(const FProcessorStats& InStats)
{
	Stats.AddStats(InStats);
}

void FReachabilityAnalysisState::ResetWorkers()
{
	NumWorkers = 0;
}

void FReachabilityAnalysisState::FinishIteration()
{
	NumIterations++;
}

bool FReachabilityAnalysisState::CheckIfAnyContextIsSuspended()
{
	bIsSuspended = false;
	for (int32 WorkerIndex = 0; WorkerIndex < NumWorkers && !bIsSuspended; ++WorkerIndex)
	{
		bIsSuspended = Contexts[WorkerIndex]->bIsSuspended;
	}
	return bIsSuspended;
}

} // namespce UE::GC

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace UE::GC::Private
{

FString FIterationTimerStat::ToString() const
{
	return FString::Printf(TEXT("%7.3fms, Iterations: %4d, Max: %7.3fms (%4d)"), Total * 1000, NumIterations, Max * 1000, SlowestIteration);
}

void FStats::DumpToLog()
{	
#define UE_TRUE_FALSE(b) (b ? TEXT("true") : TEXT("false"))
	UE_LOG(LogGarbage, Log, TEXT("Garbage Collection Total  %7.3fms%s"), TotalTime * 1000, bInProgress ? TEXT(" (still in progress, results may be incomplete)") : TEXT(""));
	UE_LOG(LogGarbage, Log, TEXT("  Verify                  %7.3fms"), VerifyTime * 1000);	
	UE_LOG(LogGarbage, Log, TEXT("  Reachability            %s"), *ReachabilityTime.ToString());
	UE_LOG(LogGarbage, Log, TEXT("    MarkAsUnreachable     %7.3fms"), MarkObjectsAsUnreachableTime * 1000);
	UE_LOG(LogGarbage, Log, TEXT("    Reference Collection  %s"), *ReferenceCollectionTime.ToString());
	UE_LOG(LogGarbage, Log, TEXT("  GarbageTracking         %7.3fms"), GarbageTrackingTime * 1000);
	UE_LOG(LogGarbage, Log, TEXT("  DissolveClusters        %7.3fms"), DissolveUnreachableClustersTime * 1000);
	UE_LOG(LogGarbage, Log, TEXT("  GatherUnreachable       %s"), *GatherUnreachableTime.ToString());
	UE_LOG(LogGarbage, Log, TEXT("  NotifyUnreachable       %7.3fms"), NotifyUnreachableTime * 1000);
	UE_LOG(LogGarbage, Log, TEXT("  VerifyNoUnreachable     %7.3fms"), VerifyNoUnreachableTime * 1000);	
	UE_LOG(LogGarbage, Log, TEXT("  Unhashing               %s"), *UnhashingTime.ToString());
	UE_LOG(LogGarbage, Log, TEXT("  DestroyGarbage          %s"), *DestroyGarbageTime.ToString());

	UE_LOG(LogGarbage, Log, TEXT("Pre GC:     Objects: %7d, Roots : %7d, Clusters : %7d, Clustered Objects : %7d"), NumObjects, NumRoots, NumClusters, NumClusteredObjects);
	UE_LOG(LogGarbage, Log, TEXT("Destroyed:  Objects: %7d, including        Clusters : %7d, Clustered Objects : %7d"), NumUnreachableObjects, NumDissolvedClusters, NumUnreachableClusteredObjects);
	UE_LOG(LogGarbage, Log, TEXT("Number of barrier objects: %d"), NumBarrierObjects);
	UE_LOG(LogGarbage, Log, TEXT("Number of weak references for clearing %d and objects that need weak reference clearing: %d"), NumWeakReferencesForClearing, NumObjectsThatNeedWeakReferenceClearing);
	UE_LOG(LogGarbage, Log, TEXT("Started as full purge: %s, finished as full purge: %s"), UE_TRUE_FALSE(bStartedAsFullPurge), UE_TRUE_FALSE(bFinishedAsFullPurge));
	UE_LOG(LogGarbage, Log, TEXT("Flushed async loading: %s"), UE_TRUE_FALSE(bFlushedAsyncLoading));
	UE_LOG(LogGarbage, Log, TEXT("Purged previous GC objects: %s"), UE_TRUE_FALSE(bPurgedPreviousGCObjects));
#undef UE_TRUE_FALSE
}

} // namespce UE::GC::Private
