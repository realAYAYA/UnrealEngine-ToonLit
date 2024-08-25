// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/CookRequestCluster.h"

#include "Algo/AllOf.h"
#include "Algo/BinarySearch.h"
#include "Algo/Sort.h"
#include "Algo/TopologicalSort.h"
#include "Algo/Unique.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/Async.h"
#include "Cooker/CookPackageData.h"
#include "Cooker/CookPlatformManager.h"
#include "Cooker/CookProfiling.h"
#include "Cooker/CookRequests.h"
#include "Cooker/CookTypes.h"
#include "Cooker/PackageTracker.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "EditorDomain/EditorDomainUtils.h"
#include "Engine/AssetManager.h"
#include "Engine/Level.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/RedirectCollector.h"
#include "Misc/ReverseIterate.h"
#include "Misc/ScopeExit.h"
#include "Misc/StringBuilder.h"
#include "String/Find.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SavePackage.h"

namespace UE::Cook
{

FRequestCluster::FRequestCluster(UCookOnTheFlyServer& InCOTFS)
	: COTFS(InCOTFS)
	, PackageDatas(*InCOTFS.PackageDatas)
	, AssetRegistry(*IAssetRegistry::Get())
	, PackageTracker(*InCOTFS.PackageTracker)
	, BuildDefinitions(*InCOTFS.BuildDefinitions)
{
	// CookByTheBookOptions is always available; in other modes it is set to the default values
	UE::Cook::FCookByTheBookOptions& Options = *COTFS.CookByTheBookOptions;
	bAllowHardDependencies = !Options.bSkipHardReferences;
	bAllowSoftDependencies = !Options.bSkipSoftReferences;
	bErrorOnEngineContentUse = Options.bErrorOnEngineContentUse;
	if (COTFS.IsCookOnTheFlyMode())
	{
		// Do not queue soft-dependencies during CookOnTheFly; wait for them to be requested
		// TODO: Report soft dependencies separately, and mark them as normal priority,
		// and mark all hard dependencies as high priority in cook on the fly.
		bAllowSoftDependencies = false;
	}

	if (bErrorOnEngineContentUse)
	{
		DLCPath = FPaths::Combine(*COTFS.GetBaseDirectoryForDLC(), TEXT("Content"));
		FPaths::MakeStandardFilename(DLCPath);
	}
	GConfig->GetBool(TEXT("CookSettings"), TEXT("PreQueueBuildDefinitions"), bPreQueueBuildDefinitions, GEditorIni);

	bAllowIterativeResults = true;
	bool bFirst = true;
	for (const ITargetPlatform* TargetPlatform : COTFS.PlatformManager->GetSessionPlatforms())
	{
		FPlatformData* PlatformData = COTFS.PlatformManager->GetPlatformData(TargetPlatform);
		if (bFirst)
		{
			bAllowIterativeResults = PlatformData->bAllowIterativeResults;
			bFirst = false;
		}
		else
		{
			if (PlatformData->bAllowIterativeResults != bAllowIterativeResults)
			{
				UE_LOG(LogCook, Warning, TEXT("Full build is requested for some platforms but not others, but this is not supported. All platforms will be built full."));
				bAllowIterativeResults = false;
			}
		}
	}
}

FRequestCluster::FRequestCluster(UCookOnTheFlyServer& InCOTFS, TArray<FFilePlatformRequest>&& InRequests)
	: FRequestCluster(InCOTFS)
{
	ReserveInitialRequests(InRequests.Num());
	FilePlatformRequests = MoveTemp(InRequests);
	InRequests.Empty();
}

FRequestCluster::FRequestCluster(UCookOnTheFlyServer& InCOTFS, FPackageDataSet&& InRequests)
	: FRequestCluster(InCOTFS)
{
	ReserveInitialRequests(InRequests.Num());
	for (FPackageData* PackageData : InRequests)
	{
		ESuppressCookReason& Existing = OwnedPackageDatas.FindOrAdd(PackageData, ESuppressCookReason::Invalid);
		check(Existing == ESuppressCookReason::Invalid);
		Existing = ESuppressCookReason::NotSuppressed;
	}
	InRequests.Empty();
}

FRequestCluster::FRequestCluster(UCookOnTheFlyServer& InCOTFS, TRingBuffer<FDiscoveryQueueElement>& DiscoveryQueue)
	: FRequestCluster(InCOTFS)
{
	TArray<const ITargetPlatform*, TInlineAllocator<ExpectedMaxNumPlatforms>> BufferPlatforms;
	if (!COTFS.bSkipOnlyEditorOnly)
	{
		BufferPlatforms = COTFS.PlatformManager->GetSessionPlatforms();
		BufferPlatforms.Add(CookerLoadingPlatformKey);
	}

	while (!DiscoveryQueue.IsEmpty())
	{
		FDiscoveryQueueElement* Discovery = &DiscoveryQueue.First();
		FPackageData& PackageData = *Discovery->PackageData;

		TConstArrayView<const ITargetPlatform*> NewReachablePlatforms;
		if (COTFS.bSkipOnlyEditorOnly)
		{
			NewReachablePlatforms = Discovery->ReachablePlatforms.GetPlatforms(COTFS, &Discovery->Instigator,
				TConstArrayView<const ITargetPlatform*>(), &BufferPlatforms);
		}
		else
		{
			NewReachablePlatforms = BufferPlatforms;
		}
		if (Discovery->Instigator.Category == EInstigator::ForceExplorableSaveTimeSoftDependency)
		{
			// This package was possibly previously marked as not explorable, but now it is marked as explorable.
			// One example of this is externalactor packages - they are by default not cookable and not explorable
			// (see comment in FRequestCluster::IsRequestCookable). But once WorldPartition loads them, we need to mark
			// them as explored so that their imports are marked as expected and all of their soft dependencies
			// are included.
			for (const ITargetPlatform* TargetPlatform : NewReachablePlatforms)
			{
				if (TargetPlatform != CookerLoadingPlatformKey)
				{
					PackageData.FindOrAddPlatformData(TargetPlatform).MarkAsExplorable();
				}
			}
		}

		if (PackageData.HasReachablePlatforms(NewReachablePlatforms))
		{
			// If there are no new reachable platforms, add it to the cluster for cooking if it needs
			// it, otherwise let it remain where it is
			FDiscoveryQueueElement PoppedDiscovery = DiscoveryQueue.PopFrontValue();
			Discovery = &PoppedDiscovery;
			if (!PackageData.IsInProgress() && PackageData.GetPlatformsNeedingCookingNum() == 0)
			{
				PackageData.SendToState(EPackageState::Request, ESendFlags::QueueRemove, EStateChangeReason::RequestCluster);
				OwnedPackageDatas.Add(&PackageData, ESuppressCookReason::NotSuppressed);
			}
			continue;
		}

		// Startup packages and Generated packages do not need to add hidden dependencies or log warnings
		if (Discovery->Instigator.Category != EInstigator::StartupPackage &&
			Discovery->Instigator.Category != EInstigator::GeneratedPackage)
		{
			// If there are other discovered packages we have already added to this cluster, then defer this one
			// until we have explored those; add this one to the next cluster. Exploring those earlier discoveries
			// might add this one through cluster exploration and not require a hidden dependency.
			if (!OwnedPackageDatas.IsEmpty())
			{
				break;
			}

			if (Discovery->ReachablePlatforms.GetSource() == EDiscoveredPlatformSet::CopyFromInstigator)
			{
				// Add it as a hidden dependency so that future platforms discovered as reachable in
				// the instigator will also be marked as reachable in the dependency.
				if (COTFS.bSkipOnlyEditorOnly)
				{
					FPackageData* InstigatorPackageData = Discovery->Instigator.Referencer.IsNone() ? nullptr
						: COTFS.PackageDatas->TryAddPackageDataByPackageName(Discovery->Instigator.Referencer);
					if (InstigatorPackageData)
					{
						COTFS.DiscoveredDependencies.FindOrAdd(InstigatorPackageData->GetPackageName())
							.Add(PackageData.GetPackageName());
					}
				}
			}

			// Adding packages to the cook should happen only for a few types of instigators, from external package
			// requests, or during cluster exploration. If not expected, add a diagnostic message.
			if (Discovery->Instigator.Category != EInstigator::SaveTimeHardDependency &&
				Discovery->Instigator.Category != EInstigator::SaveTimeSoftDependency &&
				Discovery->Instigator.Category != EInstigator::ForceExplorableSaveTimeSoftDependency)
			{
				COTFS.OnDiscoveredPackageDebug(PackageData.GetPackageName(), Discovery->Instigator);
			}
		}
		// Add the new reachable platforms
		PackageData.AddReachablePlatforms(*this, NewReachablePlatforms, MoveTemp(Discovery->Instigator));

		// Pop it off the list; note that this invalidates the pointers we had into the DiscoveryQueueElement
		FDiscoveryQueueElement PoppedDiscovery = DiscoveryQueue.PopFrontValue();
		Discovery = &PoppedDiscovery;
		NewReachablePlatforms = TConstArrayView<const ITargetPlatform*>();

		// Send it to the Request state if it's not already there, remove it from its old container
		// and add it to this cluster.
		PackageData.SendToState(EPackageState::Request, ESendFlags::QueueRemove, EStateChangeReason::RequestCluster);
		PackageData.AddUrgency(Discovery->bUrgent, false /* bAllowUpdateState */);
		OwnedPackageDatas.Add(&PackageData, ESuppressCookReason::NotSuppressed);
	}
}

FName GInstigatorRequestCluster(TEXT("RequestCluster"));

void FRequestCluster::Process(const FCookerTimer& CookerTimer, bool& bOutComplete)
{
	bOutComplete = true;

	FetchPackageNames(CookerTimer, bOutComplete);
	if (!bOutComplete)
	{
		return;
	}
	PumpExploration(CookerTimer, bOutComplete);
	if (!bOutComplete)
	{
		return;
	}
	StartAsync(CookerTimer, bOutComplete);
}

void FRequestCluster::FetchPackageNames(const FCookerTimer& CookerTimer, bool& bOutComplete)
{
	if (bPackageNamesComplete)
	{
		return;
	}

	constexpr int32 TimerCheckPeriod = 100; // Do not incur the cost of checking the timer on every package
	int32 NextRequest = 0;
	for (; NextRequest < FilePlatformRequests.Num(); ++NextRequest)
	{
		if ((NextRequest+1) % TimerCheckPeriod == 0 && CookerTimer.IsActionTimeUp())
		{
			break;
		}

		FFilePlatformRequest& Request = FilePlatformRequests[NextRequest];
		FName OriginalName = Request.GetFilename();

		// The input filenames are normalized, but might be missing their extension, so allow PackageDatas
		// to correct the filename if the package is found with a different filename
		bool bExactMatchRequired = false;
		FPackageData* PackageData = PackageDatas.TryAddPackageDataByStandardFileName(OriginalName, bExactMatchRequired);
		if (!PackageData)
		{
			LogCookerMessage(FString::Printf(TEXT("Could not find package at file %s!"),
				*OriginalName.ToString()), EMessageSeverity::Error);
			UE_LOG(LogCook, Error, TEXT("Could not find package at file %s!"), *OriginalName.ToString());
			FCompletionCallback CompletionCallback(MoveTemp(Request.GetCompletionCallback()));
			if (CompletionCallback)
			{
				CompletionCallback(nullptr);
			}
			continue;
		}

		// If it has new reachable platforms we definitely need to explore it
		if (!PackageData->HasReachablePlatforms(Request.GetPlatforms()))
		{
			PackageData->AddReachablePlatforms(*this, Request.GetPlatforms(), MoveTemp(Request.GetInstigator()));
			PullIntoCluster(*PackageData);
			PackageData->AddUrgency(Request.IsUrgent(), false /* bAllowUpdateState */);
		}
		else
		{
			if (PackageData->IsInProgress())
			{
				// If it's already in progress with no new platforms, we don't need to add it to the cluster, but add
				// add on our urgency setting
				PackageData->AddUrgency(Request.IsUrgent(), true /* bAllowUpdateState */);
			}
			else if (PackageData->GetPlatformsNeedingCookingNum() > 0)
			{
				// If it's missing cookable platforms and not in progress we need to add it to the cluster for cooking
				PullIntoCluster(*PackageData);
				PackageData->AddUrgency(Request.IsUrgent(), true /* bAllowUpdateState */);
			}
		}
		// Add on our completion callback, or call it immediately if already done
		PackageData->AddCompletionCallback(Request.GetPlatforms(), MoveTemp(Request.GetCompletionCallback()));
	}
	if (NextRequest < FilePlatformRequests.Num())
	{
		FilePlatformRequests.RemoveAt(0, NextRequest);
		bOutComplete = false;
		return;
	}

	FilePlatformRequests.Empty();
	bPackageNamesComplete = true;
}

void FRequestCluster::ReserveInitialRequests(int32 RequestNum)
{
	OwnedPackageDatas.Reserve(FMath::Max(RequestNum, 1024));
}

void FRequestCluster::PullIntoCluster(FPackageData& PackageData)
{
	ESuppressCookReason& Existing = OwnedPackageDatas.FindOrAdd(&PackageData, ESuppressCookReason::Invalid);
	if (Existing == ESuppressCookReason::Invalid)
	{
		// Steal it from wherever it is and send it to Request State. It has already been added to this cluster
		if (PackageData.GetState() == EPackageState::Request)
		{
			COTFS.PackageDatas->GetRequestQueue().RemoveRequestExceptFromCluster(&PackageData, this);
		}
		else
		{
			PackageData.SendToState(EPackageState::Request, ESendFlags::QueueRemove, EStateChangeReason::RequestCluster);
		}
		Existing = ESuppressCookReason::NotSuppressed;
	}
}

void FRequestCluster::StartAsync(const FCookerTimer& CookerTimer, bool& bOutComplete)
{
	using namespace UE::DerivedData;
	using namespace UE::EditorDomain;

	if (bStartAsyncComplete)
	{
		return;
	}

	FEditorDomain* EditorDomain = FEditorDomain::Get();
	if (EditorDomain && EditorDomain->IsReadingPackages())
	{
		bool bBatchDownloadEnabled = true;
		GConfig->GetBool(TEXT("EditorDomain"), TEXT("BatchDownloadEnabled"), bBatchDownloadEnabled, GEditorIni);
		if (bBatchDownloadEnabled)
		{
			// If the EditorDomain is active, then batch-download all packages to cook from remote cache into local
			TArray<FName> BatchDownload;
			BatchDownload.Reserve(OwnedPackageDatas.Num());
			for (TPair<FPackageData*, ESuppressCookReason>& Pair : OwnedPackageDatas)
			{
				if (Pair.Value == ESuppressCookReason::NotSuppressed)
				{
					BatchDownload.Add(Pair.Key->GetPackageName());
				}
			};
			EditorDomain->BatchDownload(BatchDownload);
		}
	}

	bStartAsyncComplete = true;
}

int32 FRequestCluster::NumPackageDatas() const
{
	return OwnedPackageDatas.Num();
}

void FRequestCluster::RemovePackageData(FPackageData* PackageData)
{
	if (OwnedPackageDatas.Remove(PackageData) == 0)
	{
		return;
	}

	if (GraphSearch)
	{
		GraphSearch->RemovePackageData(PackageData);
	}
}

void FRequestCluster::OnNewReachablePlatforms(FPackageData* PackageData)
{
	if (GraphSearch)
	{
		GraphSearch->OnNewReachablePlatforms(PackageData);
	}
}

void FRequestCluster::OnPlatformAddedToSession(const ITargetPlatform* TargetPlatform)
{
	if (GraphSearch)
	{
		FCookerTimer CookerTimer(FCookerTimer::Forever);
		bool bComplete;
		while (PumpExploration(CookerTimer, bComplete), !bComplete)
		{
			UE_LOG(LogCook, Display, TEXT("Waiting for RequestCluster to finish before adding platform to session."));
			FPlatformProcess::Sleep(.001f);
		}
	}
}

void FRequestCluster::OnRemoveSessionPlatform(const ITargetPlatform* TargetPlatform)
{
	if (GraphSearch)
	{
		FCookerTimer CookerTimer(FCookerTimer::Forever);
		bool bComplete;
		while (PumpExploration(CookerTimer, bComplete), !bComplete)
		{
			UE_LOG(LogCook, Display, TEXT("Waiting for RequestCluster to finish before removing platform from session."));
			FPlatformProcess::Sleep(.001f);
		}
	}
}

void FRequestCluster::RemapTargetPlatforms(TMap<ITargetPlatform*, ITargetPlatform*>& Remap)
{
	if (GraphSearch)
	{
		// The platforms have already been invalidated, which means we can't wait for GraphSearch to finish
		// Need to wait for all async operations to finish, then remap all the platforms
		checkNoEntry(); // Not yet implemented
	}
}

bool FRequestCluster::Contains(FPackageData* PackageData) const
{
	return OwnedPackageDatas.Contains(PackageData);
}

void FRequestCluster::ClearAndDetachOwnedPackageDatas(TArray<FPackageData*>& OutRequestsToLoad,
	TArray<TPair<FPackageData*, ESuppressCookReason>>& OutRequestsToDemote,
	TMap<FPackageData*, TArray<FPackageData*>>& OutRequestGraph)
{
	if (bStartAsyncComplete)
	{
		check(!GraphSearch);
		OutRequestsToLoad.Reset();
		OutRequestsToDemote.Reset();
		for (TPair<FPackageData*, ESuppressCookReason>& Pair : OwnedPackageDatas)
		{
			if (Pair.Value == ESuppressCookReason::NotSuppressed)
			{
				OutRequestsToLoad.Add(Pair.Key);
			}
			else
			{
				OutRequestsToDemote.Add(Pair);
			}
		}
		OutRequestGraph = MoveTemp(RequestGraph);
	}
	else
	{
		OutRequestsToLoad.Reset();
		for (TPair<FPackageData*, ESuppressCookReason>& Pair : OwnedPackageDatas)
		{
			OutRequestsToLoad.Add(Pair.Key);
		}
		OutRequestsToDemote.Reset();
		OutRequestGraph.Reset();
	}
	FilePlatformRequests.Empty();
	OwnedPackageDatas.Empty();
	GraphSearch.Reset();
	RequestGraph.Reset();
}

void FRequestCluster::PumpExploration(const FCookerTimer& CookerTimer, bool& bOutComplete)
{
	if (bDependenciesComplete)
	{
		return;
	}

	if (!GraphSearch)
	{
		ETraversalTier TraversalTier = ETraversalTier::None;
		if (COTFS.IsCookWorkerMode())
		{
			TraversalTier = ETraversalTier::None;
		}
		else
		{
			TraversalTier = bAllowHardDependencies ? ETraversalTier::FollowDependencies : ETraversalTier::FetchEdgeData;
		}
		GraphSearch.Reset(new FGraphSearch(*this, TraversalTier));

		if (TraversalTier == ETraversalTier::None)
		{
			GraphSearch->VisitWithoutDependencies();
			GraphSearch.Reset();
			bDependenciesComplete = true;
			return;
		}
		GraphSearch->StartSearch();
	}

	constexpr double WaitTime = 0.50;
	bool bDone;
	while (GraphSearch->TickExploration(bDone), !bDone)
	{
		GraphSearch->WaitForAsyncQueue(WaitTime);
		if (CookerTimer.IsActionTimeUp())
		{
			bOutComplete = false;
			return;
		}
	}

	TArray<FPackageData*> SortedPackages;
	SortedPackages.Reserve(OwnedPackageDatas.Num());
	for (TPair<FPackageData*, ESuppressCookReason>& Pair : OwnedPackageDatas)
	{
		if (Pair.Value == ESuppressCookReason::NotSuppressed)
		{
			SortedPackages.Add(Pair.Key);
		}
	}

	// Sort the NewRequests in leaf to root order and replace the requests list with NewRequests
	TArray<FPackageData*> Empty;
	auto GetElementDependencies = [this, &Empty](FPackageData* PackageData) -> const TArray<FPackageData*>&
	{
		const TArray<FPackageData*>* VertexEdges = GraphSearch->GetGraphEdges().Find(PackageData);
		return VertexEdges ? *VertexEdges : Empty;
	};

	Algo::TopologicalSort(SortedPackages, GetElementDependencies, Algo::ETopologicalSort::AllowCycles);
	if (COTFS.bRandomizeCookOrder)
	{
		RandomizeCookOrder(SortedPackages, GraphSearch->GetGraphEdges());
		// Only shuffle the first cluster. We don't need to spend time shuffling later clusters because they
		// will already occur at random times, and we want to suppress the log information about them since
		// there can be a lot of small discovered clusters.
		COTFS.bRandomizeCookOrder = false;
	}
	TMap<FPackageData*, int32> SortOrder;
	int32 Counter = 0;
	SortOrder.Reserve(SortedPackages.Num());
	for (FPackageData* PackageData : SortedPackages)
	{
		SortOrder.Add(PackageData, Counter++);
	}
	OwnedPackageDatas.KeySort([&SortOrder](const FPackageData& A, const FPackageData& B)
		{
			int32* CounterA = SortOrder.Find(&A);
			int32* CounterB = SortOrder.Find(&B);
			if ((CounterA != nullptr) != (CounterB != nullptr))
			{
				// Sort the demotes to occur last
				return CounterB == nullptr;
			}
			else if (CounterA)
			{
				return *CounterA < *CounterB;
			}
			else
			{
				return false; // demotes are unsorted
			}
		});

	RequestGraph = MoveTemp(GraphSearch->GetGraphEdges());
	GraphSearch.Reset();
	bDependenciesComplete = true;
}

FRequestCluster::FGraphSearch::FGraphSearch(FRequestCluster& InCluster, ETraversalTier InTraversalTier)
	: Cluster(InCluster)
	, TraversalTier(InTraversalTier)
	, AsyncResultsReadyEvent(EEventMode::ManualReset)
{
	AsyncResultsReadyEvent->Trigger();
	LastActivityTime = FPlatformTime::Seconds();
	VertexAllocator.SetMaxBlockSize(1024);
	VertexAllocator.SetMaxBlockSize(65536);
	VertexQueryAllocator.SetMinBlockSize(1024);
	VertexQueryAllocator.SetMaxBlockSize(1024);
	BatchAllocator.SetMaxBlockSize(16);
	BatchAllocator.SetMaxBlockSize(16);

	TConstArrayView<const ITargetPlatform*> SessionPlatforms = Cluster.COTFS.PlatformManager->GetSessionPlatforms();
	check(SessionPlatforms.Num() > 0);
	FetchPlatforms.SetNum(SessionPlatforms.Num() + 2);
	FetchPlatforms[PlatformAgnosticPlatformIndex].bIsPlatformAgnosticPlatform = true;
	FetchPlatforms[CookerLoadingPlatformIndex].Platform = CookerLoadingPlatformKey;
	FetchPlatforms[CookerLoadingPlatformIndex].bIsCookerLoadingPlatform = true;
	for (int32 SessionPlatformIndex = 0; SessionPlatformIndex < SessionPlatforms.Num(); ++SessionPlatformIndex)
	{
		FFetchPlatformData& FetchPlatform = FetchPlatforms[SessionPlatformIndex + 2];
		FetchPlatform.Platform = SessionPlatforms[SessionPlatformIndex];
		FetchPlatform.Writer = &Cluster.COTFS.FindOrCreatePackageWriter(FetchPlatform.Platform);
	}
	Algo::Sort(FetchPlatforms, [](const FFetchPlatformData& A, const FFetchPlatformData& B)
		{
			return A.Platform < B.Platform;
		});
	check(FetchPlatforms[PlatformAgnosticPlatformIndex].bIsPlatformAgnosticPlatform);
	check(FetchPlatforms[CookerLoadingPlatformIndex].bIsCookerLoadingPlatform);
}

void FRequestCluster::FGraphSearch::VisitWithoutDependencies()
{
	// PumpExploration is responsible for marking all requests as explored and cookable/uncoookable.
	// If we're skipping the dependencies search, handle that responsibility for the initial requests and return.
	for (TPair<FPackageData*, ESuppressCookReason>& Pair : Cluster.OwnedPackageDatas)
	{
		FVertexData Vertex;
		Vertex.PackageData = Pair.Key;
		VisitVertex(Vertex);
	}
}

void FRequestCluster::FGraphSearch::StartSearch()
{
	Frontier.Reserve(Cluster.OwnedPackageDatas.Num());
	for (TPair<FPackageData*, ESuppressCookReason>& Pair : Cluster.OwnedPackageDatas)
	{
		FVertexData& Vertex = FindOrAddVertex(Pair.Key->GetPackageName(), *Pair.Key);
		check(Vertex.PackageData);
		// We're iterating over OwnedPackageDatas and the Vertex is already in the Cluster so we don't need to call AddToFrontier;
		// just add it directly.
		check(Pair.Value != ESuppressCookReason::Invalid);
		Frontier.Add(&Vertex);
	}
}

FRequestCluster::FGraphSearch::~FGraphSearch()
{
	for (;;)
	{
		bool bHadActivity = false;
		bool bAsyncBatchesEmpty = false;
		{
			FScopeLock ScopeLock(&Lock);
			bAsyncBatchesEmpty = AsyncQueueBatches.IsEmpty();
			if (!bAsyncBatchesEmpty)
			{
				// It is safe to Reset AsyncResultsReadyEvent and wait on it later because we are inside the lock and there is a
				// remaining batch, so it will be triggered after the Reset when that batch completes.
				AsyncResultsReadyEvent->Reset();
			}
		}
		for (;;)
		{
			TOptional<FVertexData*> Vertex = AsyncQueueResults.Dequeue();
			if (!Vertex)
			{
				break;
			}
			FreeQueryData((**Vertex).QueryData);
			(**Vertex).QueryData = nullptr;
			bHadActivity = true;
		}
		if (bAsyncBatchesEmpty)
		{
			break;
		}
		if (bHadActivity)
		{
			LastActivityTime = FPlatformTime::Seconds();
		}
		else
		{
			UpdateDisplay();
		}
		constexpr double WaitTime = 1.0;
		WaitForAsyncQueue(WaitTime);
	}
}

void FRequestCluster::FGraphSearch::RemovePackageData(FPackageData* PackageData)
{
	check(PackageData);
	FVertexData** Vertex = Vertices.Find(PackageData->GetPackageName());
	if (Vertex)
	{
		(**Vertex).PackageData = nullptr;
	}

	GraphEdges.Remove(PackageData);
	for (TPair<FPackageData*, TArray<FPackageData*>>& Pair : GraphEdges)
	{
		Pair.Value.Remove(PackageData);
	}
}

void FRequestCluster::FGraphSearch::OnNewReachablePlatforms(FPackageData* PackageData)
{
	FVertexData** VertexPtr = Vertices.Find(PackageData->GetPackageName());
	if (!VertexPtr)
	{
		return;
	}
	// Already in OwnedPackageDatas, so just add to Frontier directly
	Frontier.Add(*VertexPtr);
}

void FRequestCluster::FGraphSearch::QueueEdgesFetch(FVertexData& Vertex, TConstArrayView<const ITargetPlatform*> Platforms)
{
	check(!Vertex.QueryData);
	FVertexQueryData& QueryData = *AllocateQueryData();
	Vertex.QueryData = &QueryData;
	
	QueryData.PackageName = Vertex.PackageData->GetPackageName();
	QueryData.Platforms.SetNum(FetchPlatforms.Num());

	// Store Platforms in QueryData->Platforms.bActive. All bActive values start false from constructor or from Reset
	bool bHasPlatformAgnostic = false;
	for (const ITargetPlatform* Platform : Platforms)
	{
		int32 Index = Algo::BinarySearchBy(FetchPlatforms, Platform, [](const FFetchPlatformData& D) { return D.Platform; });
		check(Index != INDEX_NONE);
		QueryData.Platforms[Index].bActive = true;
		if (Platform != CookerLoadingPlatformKey)
		{
			bHasPlatformAgnostic = true;
		}
	}
	if (bHasPlatformAgnostic)
	{
		QueryData.Platforms[PlatformAgnosticPlatformIndex].bActive = true;
	}
	int32 NumPendingPlatforms = Platforms.Num() + (bHasPlatformAgnostic ? 1 : 0);
	QueryData.PendingPlatforms.store(NumPendingPlatforms, std::memory_order_release);

	PreAsyncQueue.Add(&Vertex);
	CreateAvailableBatches(false /* bAllowIncompleteBatch */);
}

void FRequestCluster::FGraphSearch::WaitForAsyncQueue(double WaitTimeSeconds)
{
	uint32 WaitTime = (WaitTimeSeconds > 0.0) ? static_cast<uint32>(FMath::Floor(WaitTimeSeconds * 1000)) : MAX_uint32;
	AsyncResultsReadyEvent->Wait(WaitTime);
}

void FRequestCluster::FGraphSearch::TickExploration(bool& bOutDone)
{
	bool bHadActivity = false;
	for (;;)
	{
		TOptional<FVertexData*> Vertex = AsyncQueueResults.Dequeue();
		if (!Vertex.IsSet())
		{
			break;
		}
		ExploreVertexEdges(**Vertex);
		FreeQueryData((**Vertex).QueryData);
		(**Vertex).QueryData = nullptr;
		bHadActivity = true;
	}

	if (!Frontier.IsEmpty())
	{
		TArray<FVertexData*> BusyVertices;
		for (FVertexData* Vertex : Frontier)
		{
			if (Vertex->QueryData)
			{
				// Vertices that are already in the AsyncQueue can not be added again; we would clobber their QueryData. Postpone them.
				BusyVertices.Add(Vertex);
			}
			else
			{
				VisitVertex(*Vertex);
			}
		}
		bHadActivity |= BusyVertices.Num() != Frontier.Num();
		Frontier.Reset();
		Frontier.Append(BusyVertices);
	}

	if (bHadActivity)
	{
		LastActivityTime = FPlatformTime::Seconds();
		bOutDone = false;
		return;
	}

	bool bAsyncQueueEmpty;
	{
		FScopeLock ScopeLock(&Lock);
		if (!AsyncQueueResults.IsEmpty())
		{
			bAsyncQueueEmpty = false;
		}
		else
		{
			bAsyncQueueEmpty = AsyncQueueBatches.IsEmpty();
			// AsyncResultsReadyEvent can only be Reset when either the AsyncQueue is empty or it is non-empty and we
			// know the AsyncResultsReadyEvent will be triggered again "later".
			// The guaranteed place where it will be Triggered is when a batch completes. To guarantee that
			// place will be called "later", the batch completion trigger and this reset have to both
			// be done inside the lock.
			AsyncResultsReadyEvent->Reset();
		}
	}
	if (!bAsyncQueueEmpty)
	{
		// Waiting on the AsyncQueue; give a warning if we have been waiting for long with no AsyncQueueResults.
		UpdateDisplay();
		bOutDone = false;
		return;
	}

	// No more work coming in the future from the AsyncQueue, and we are out of work to do
	// without it. If we have any queued vertices in the PreAsyncQueue, send them now and continue
	// waiting. Otherwise we are done.
	if (!PreAsyncQueue.IsEmpty())
	{
		CreateAvailableBatches(true /* bAllowInCompleteBatch */);
		bOutDone = false;
		return;
	}

	// Frontier was reset above, and it cannot be modified between there and here.
	// If it were non-empty we would not be done.
	check(Frontier.IsEmpty());
	bOutDone = true;
}

void FRequestCluster::FGraphSearch::UpdateDisplay()
{
	constexpr double WarningTimeout = 10.0;
	if (FPlatformTime::Seconds() > LastActivityTime + WarningTimeout && Cluster.IsIncrementalCook())
	{
		FScopeLock ScopeLock(&Lock);
		int32 NumVertices = 0;
		int32 NumBatches = AsyncQueueBatches.Num();
		for (FQueryVertexBatch* Batch : AsyncQueueBatches)
		{
			NumVertices += Batch->PendingVertices;
		}

		UE_LOG(LogCook, Warning, TEXT("FRequestCluster waited more than %.0lfs for previous build results from the oplog. ")
			TEXT("NumPendingBatches == %d, NumPendingVertices == %d. Continuing to wait..."),
			WarningTimeout, NumBatches, NumVertices);
		LastActivityTime = FPlatformTime::Seconds();
	}
}

void FRequestCluster::FGraphSearch::VisitVertex(FVertexData& Vertex)
{
	// Only called from PumpExploration thread

	// The PackageData will not exist if the package does not exist on disk or
	// the PackageData was removed from the FRequestCluster due to changes in the PackageData's
	// state elsewhere in the cooker.
	if (!Vertex.PackageData)
	{
		return;
	}

	TArray<const ITargetPlatform*, TInlineAllocator<1>> ExplorePlatforms;
	FPackagePlatformData* CookerLoadingPlatform = nullptr;
	const ITargetPlatform* FirstReachableSessionPlatform = nullptr;
	ESuppressCookReason SuppressCookReason = ESuppressCookReason::Invalid;
	bool bAllReachablesUncookable = true;
	for (TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : Vertex.PackageData->GetPlatformDatasConstKeysMutableValues())
	{
		FPackagePlatformData& PlatformData = Pair.Value;
		if (Pair.Key == CookerLoadingPlatformKey)
		{
			CookerLoadingPlatform = &Pair.Value;
		}
		else if (PlatformData.IsReachable())
		{
			if (!FirstReachableSessionPlatform)
			{
				FirstReachableSessionPlatform = Pair.Key;
			}
			if (!PlatformData.IsVisitedByCluster())
			{
				VisitVertexForPlatform(Vertex, Pair.Key, PlatformData, SuppressCookReason);
				if ((TraversalTier >= ETraversalTier::FetchEdgeData) && 
					(((TraversalTier >= ETraversalTier::FollowDependencies) && PlatformData.IsExplorable())
						|| Cluster.IsIncrementalCook()))
				{
					ExplorePlatforms.Add(Pair.Key);
				}
			}
			if (PlatformData.IsCookable())
			{
				bAllReachablesUncookable = false;
				SuppressCookReason = ESuppressCookReason::NotSuppressed;
			}
		}
	}
	bool bAnyCookable = (FirstReachableSessionPlatform == nullptr) | !bAllReachablesUncookable;
	if (bAnyCookable != Vertex.bAnyCookable)
	{
		if (!bAnyCookable)
		{
			if (SuppressCookReason == ESuppressCookReason::Invalid)
			{
				// We need the SuppressCookReason for reporting. If we didn't calculate it this Visit and
				// we don't have it stored in this->OwnedPackageDatas, then we must have calculated it in
				// a previous cluster, but we don't store it anywhere. Recalculate it from the
				// FirstReachableSessionPlatform. FirstReachableSessionPlatform must be non-null, otherwise
				// bAnyCookable would be true.
				check(FirstReachableSessionPlatform);
				bool bCookable;
				bool bExplorable;
				Cluster.IsRequestCookable(FirstReachableSessionPlatform, Vertex.PackageData->GetPackageName(),
					*Vertex.PackageData, SuppressCookReason, bCookable, bExplorable);
				check(!bCookable); // We don't support bCookable changing for a given package and platform
				check(SuppressCookReason != ESuppressCookReason::Invalid);
			}
		}
		else
		{
			check(SuppressCookReason == ESuppressCookReason::NotSuppressed);
		}
		Cluster.OwnedPackageDatas.FindOrAdd(Vertex.PackageData) = SuppressCookReason;
		Vertex.bAnyCookable = bAnyCookable;
	}

	// If any target platform is cookable, then we need to mark the CookerLoadingPlatform as reachable because we will need
	// to load the package to cook it
	if (bAnyCookable)
	{
		if (!CookerLoadingPlatform)
		{
			CookerLoadingPlatform = &Vertex.PackageData->FindOrAddPlatformData(CookerLoadingPlatformKey);
		}
		CookerLoadingPlatform->SetReachable(true);
	}
	if (CookerLoadingPlatform && CookerLoadingPlatform->IsReachable() && !CookerLoadingPlatform->IsVisitedByCluster())
	{
		CookerLoadingPlatform->SetCookable(true);
		CookerLoadingPlatform->SetExplorable(true);
		CookerLoadingPlatform->SetVisitedByCluster(true);
		if (TraversalTier >= ETraversalTier::FollowDependencies)
		{
			ExplorePlatforms.Add(CookerLoadingPlatformKey);
		}
	}

	if (!ExplorePlatforms.IsEmpty() && TraversalTier >= ETraversalTier::FetchEdgeData)
	{
		QueueEdgesFetch(Vertex, ExplorePlatforms);
	}
}

void FRequestCluster::FGraphSearch::VisitVertexForPlatform(FVertexData& Vertex, const ITargetPlatform* Platform,
	FPackagePlatformData& PlatformData, ESuppressCookReason& AccumulatedSuppressCookReason)
{
	FPackageData& PackageData = *Vertex.PackageData;
	ESuppressCookReason SuppressCookReason = ESuppressCookReason::Invalid;
	bool bCookable;
	bool bExplorable;
	Cluster.IsRequestCookable(Platform, Vertex.PackageData->GetPackageName(), PackageData, SuppressCookReason,
		bCookable, bExplorable);
	PlatformData.SetCookable(bCookable);
	PlatformData.SetExplorable(bExplorable);
	if (bCookable)
	{
		AccumulatedSuppressCookReason = ESuppressCookReason::NotSuppressed;
	}
	else
	{
		check(SuppressCookReason != ESuppressCookReason::Invalid && SuppressCookReason != ESuppressCookReason::NotSuppressed);
		if (AccumulatedSuppressCookReason == ESuppressCookReason::Invalid)
		{
			AccumulatedSuppressCookReason = SuppressCookReason;
		}
	}
	PlatformData.SetVisitedByCluster(true);
}

void FRequestCluster::FGraphSearch::ExploreVertexEdges(FVertexData& Vertex)
{
	// Only called from PumpExploration thread
	using namespace UE::AssetRegistry;
	using namespace UE::TargetDomain;

	// The PackageData will not exist if the package does not exist on disk or
	// the PackageData was removed from the FRequestCluster due to changes in the PackageData's
	// state elsewhere in the cooker.
	if (!Vertex.PackageData)
	{
		return;
	}

	TArray<FName>& HardGameDependencies(Scratch.HardGameDependencies);
	TArray<FName>& HardEditorDependencies(Scratch.HardEditorDependencies);
	TArray<FName>& SoftGameDependencies(Scratch.SoftGameDependencies);
	TSet<FName>& HardDependenciesSet(Scratch.HardDependenciesSet);
	HardGameDependencies.Reset();
	HardEditorDependencies.Reset();
	SoftGameDependencies.Reset();
	HardDependenciesSet.Reset();
	FPackageData& PackageData = *Vertex.PackageData;
	FName PackageName = PackageData.GetPackageName();
	bool bFetchAnyTargetPlatform = Vertex.QueryData->Platforms[PlatformAgnosticPlatformIndex].bActive;
	TArray<FName>* DiscoveredDependencies = Cluster.COTFS.DiscoveredDependencies.Find(PackageName);
	if (bFetchAnyTargetPlatform)
	{
		EDependencyQuery FlagsForHardDependencyQuery;
		if (Cluster.COTFS.bSkipOnlyEditorOnly)
		{
			Cluster.AssetRegistry.GetDependencies(PackageName, HardGameDependencies, EDependencyCategory::Package,
				EDependencyQuery::Game | EDependencyQuery::Hard);
			HardDependenciesSet.Append(HardGameDependencies);
		}
		else
		{
			// We're not allowed to skip editoronly imports, so include all hard dependencies
			FlagsForHardDependencyQuery = EDependencyQuery::Hard;
			Cluster.AssetRegistry.GetDependencies(PackageName, HardGameDependencies, EDependencyCategory::Package,
				EDependencyQuery::Game | EDependencyQuery::Hard);
			Cluster.AssetRegistry.GetDependencies(PackageName, HardEditorDependencies, EDependencyCategory::Package,
				EDependencyQuery::EditorOnly | EDependencyQuery::Hard);
			HardDependenciesSet.Append(HardGameDependencies);
			HardDependenciesSet.Append(HardEditorDependencies);
		}
		if (DiscoveredDependencies)
		{
			HardDependenciesSet.Append(*DiscoveredDependencies);
		}
		if (Cluster.bAllowSoftDependencies)
		{
			// bSkipOnlyEditorOnly is always true for soft dependencies; skip editoronly soft dependencies
			Cluster.AssetRegistry.GetDependencies(PackageName, SoftGameDependencies, EDependencyCategory::Package,
				EDependencyQuery::Game | EDependencyQuery::Soft);

			// Even if we're following soft references in general, we need to check with the SoftObjectPath registry
			// for any startup packages that marked their softobjectpaths as excluded, and not follow those
			TSet<FName>& SkippedPackages(Scratch.SkippedPackages);
			if (GRedirectCollector.RemoveAndCopySoftObjectPathExclusions(PackageName, SkippedPackages))
			{
				SoftGameDependencies.RemoveAll([&SkippedPackages](FName SoftDependency)
					{
						return SkippedPackages.Contains(SoftDependency);
					});
			}

			// LocalizationReferences are a source of SoftGameDependencies that are not present in the AssetRegistry
			SoftGameDependencies.Append(GetLocalizationReferences(PackageName, Cluster.COTFS));

			// The AssetManager can provide additional SoftGameDependencies
			SoftGameDependencies.Append(GetAssetManagerReferences(PackageName));
		}
	}

	int32 LocalNumFetchPlatforms = NumFetchPlatforms();
	TMap<FName, FScratchPlatformDependencyBits>& PlatformDependencyMap(Scratch.PlatformDependencyMap);
	PlatformDependencyMap.Reset();
	auto AddPlatformDependency = [&PlatformDependencyMap, LocalNumFetchPlatforms]
	(FName DependencyName, int32 PlatformIndex, EInstigator InstigatorType)
	{
		FScratchPlatformDependencyBits& PlatformDependencyBits = PlatformDependencyMap.FindOrAdd(DependencyName);
		if (PlatformDependencyBits.HasPlatformByIndex.Num() != LocalNumFetchPlatforms)
		{
			PlatformDependencyBits.HasPlatformByIndex.Init(false, LocalNumFetchPlatforms);
			PlatformDependencyBits.InstigatorType = EInstigator::SoftDependency;
		}
		PlatformDependencyBits.HasPlatformByIndex[PlatformIndex] = true;

		// Calculate PlatformDependencyType.InstigatorType == Max(InstigatorType, PlatformDependencyType.InstigatorType)
		// based on the enum values, from least required to most: [ Soft, HardEditorOnly, Hard ]
		switch (InstigatorType)
		{
		case EInstigator::HardDependency:
			PlatformDependencyBits.InstigatorType = InstigatorType;
			break;
		case EInstigator::HardEditorOnlyDependency:
			if (PlatformDependencyBits.InstigatorType != EInstigator::HardDependency)
			{
				PlatformDependencyBits.InstigatorType = InstigatorType;
			}
			break;
		case EInstigator::SoftDependency:
			// New value is minimum, so keep the old value
			break;
		case EInstigator::InvalidCategory:
			// Caller indicated they do not want to set the InstigatorType
			break;
		default:
			checkNoEntry();
			break;
		}
	};
	auto AddPlatformDependencyRange = [&AddPlatformDependency]
	(TConstArrayView<FName> Range, int32 PlatformIndex, EInstigator InstigatorType)
	{
		for (FName DependencyName : Range)
		{
			AddPlatformDependency(DependencyName, PlatformIndex, InstigatorType);
		}
	};

	FQueryPlatformData& PlatformAgnosticQueryPlatformData = Vertex.QueryData->Platforms[PlatformAgnosticPlatformIndex];

	auto ProcessPlatformAttachments = [this, PackageName, &PackageData, &PlatformAgnosticQueryPlatformData, &AddPlatformDependencyRange]
		(int32 PlatformIndex, const ITargetPlatform* TargetPlatform, FFetchPlatformData& FetchPlatformData,
			FPackagePlatformData& PackagePlatformData, const FCookAttachments& PlatformAttachments, bool bExploreDependencies)
	{
		bool bFoundBuildDefinitions = false;
		ICookedPackageWriter* PackageWriter = FetchPlatformData.Writer;

		if (Cluster.IsIncrementalCook() && PackagePlatformData.IsCookable())
		{
			bool bIterativelyUnmodified = false;
			if (IsCookAttachmentsValid(PackageName, PlatformAttachments))
			{
				if (IsIterativeEnabled(PackageName, Cluster.COTFS.bHybridIterativeAllowAllClasses))
				{
					bIterativelyUnmodified = true;
					PackagePlatformData.SetIterativelyUnmodified(true);
				}
				if (bExploreDependencies)
				{
					AddPlatformDependencyRange(PlatformAttachments.BuildDependencies, PlatformIndex,
						EInstigator::HardDependency);
					if (Cluster.bAllowSoftDependencies)
					{
						AddPlatformDependencyRange(PlatformAttachments.RuntimeOnlyDependencies, PlatformIndex,
							EInstigator::HardDependency);
					}
				}

				if (Cluster.bPreQueueBuildDefinitions)
				{
					bFoundBuildDefinitions = true;
					Cluster.BuildDefinitions.AddBuildDefinitionList(PackageName, TargetPlatform,
						PlatformAttachments.BuildDefinitionList);
				}
			}
			bool bShouldIterativelySkip = bIterativelyUnmodified;
			PackageWriter->UpdatePackageModificationStatus(PackageName, bIterativelyUnmodified, bShouldIterativelySkip);
			if (bShouldIterativelySkip)
			{
				// Call SetPlatformCooked instead of just PackagePlatformData.SetCookResults because we might also need
				// to set OnFirstCookedPlatformAdded
				PackageData.SetPlatformCooked(TargetPlatform, ECookResult::Succeeded);
				if (PlatformIndex == FirstSessionPlatformIndex)
				{
					COOK_STAT(++DetailedCookStats::NumPackagesIterativelySkipped);
				}
				// Declare the package to the EDLCookInfo verification so we don't warn about missing exports from it
				UE::SavePackageUtilities::EDLCookInfoAddIterativelySkippedPackage(PackageName);
			}
		}

		if (Cluster.bPreQueueBuildDefinitions && !bFoundBuildDefinitions)
		{
			if (PlatformAgnosticQueryPlatformData.bActive &&
				IsCookAttachmentsValid(PackageName, PlatformAgnosticQueryPlatformData.CookAttachments))
			{
				Cluster.BuildDefinitions.AddBuildDefinitionList(PackageName, TargetPlatform,
					PlatformAgnosticQueryPlatformData.CookAttachments.BuildDefinitionList);
			}
		}
	};

	for (int32 PlatformIndex = 0; PlatformIndex < LocalNumFetchPlatforms; ++PlatformIndex)
	{
		FQueryPlatformData& QueryPlatformData = Vertex.QueryData->Platforms[PlatformIndex];
		if (!QueryPlatformData.bActive || PlatformIndex == PlatformAgnosticPlatformIndex)
		{
			continue;
		}

		FFetchPlatformData& FetchPlatformData = FetchPlatforms[PlatformIndex];
		const ITargetPlatform* TargetPlatform = FetchPlatformData.Platform;
		FPackagePlatformData& PackagePlatformData = PackageData.FindOrAddPlatformData(TargetPlatform);
		if ((TraversalTier < ETraversalTier::FollowDependencies) || !PackagePlatformData.IsExplorable())
		{
			// ExploreVertexEdges is responsible for updating package modification status so we might
			// have been called for this platform even if not explorable. If not explorable, just update
			// package modification status for the given platform, except for CookerLoadingPlatformIndex which has
			// no status to update.
			if (PlatformIndex != CookerLoadingPlatformIndex)
			{
				ProcessPlatformAttachments(PlatformIndex, TargetPlatform, FetchPlatformData, PackagePlatformData,
					QueryPlatformData.CookAttachments, false /* bExploreDependencies */);
			}
			continue;
		}

		if (PlatformIndex == CookerLoadingPlatformIndex)
		{
			TArray<FName>& CookerLoadingDependencies(Scratch.CookerLoadingDependencies);
			CookerLoadingDependencies.Reset();

			Cluster.AssetRegistry.GetDependencies(PackageName, CookerLoadingDependencies, EDependencyCategory::Package,
				EDependencyQuery::Hard);

			// ITERATIVECOOK_TODO: Build dependencies need to be stored and used to mark package loads as expected
			// But we can't use them to explore packages that will be loaded during cook because they might not be;
			// some build dependencies might be a conservative list but unused by the asset, or unused on targetplatform
			// Adding BuildDependencies also sets up many circular dependencies, because maps declare their external
			// actors as build dependencies and the external actors declare the map as a build or hard dependency.
			// Topological sort done at the end of the Cluster has poor performance when there are 100k+ circular dependencies.
			constexpr bool bAddBuildDependenciesToGraph = false;
			if (bAddBuildDependenciesToGraph)
			{
				Cluster.AssetRegistry.GetDependencies(PackageName, CookerLoadingDependencies, EDependencyCategory::Package,
					EDependencyQuery::Build);
			}
			// CookerLoadingPlatform does not cause SetInstigator so it does not modify the platformdependency's InstigatorType
			AddPlatformDependencyRange(CookerLoadingDependencies, PlatformIndex, EInstigator::InvalidCategory);
		}
		else
		{
			AddPlatformDependencyRange(HardGameDependencies, PlatformIndex, EInstigator::HardDependency);
			AddPlatformDependencyRange(HardEditorDependencies, PlatformIndex, EInstigator::HardEditorOnlyDependency);
			AddPlatformDependencyRange(SoftGameDependencies, PlatformIndex, EInstigator::SoftDependency);
			ProcessPlatformAttachments(PlatformIndex, TargetPlatform, FetchPlatformData, PackagePlatformData,
				QueryPlatformData.CookAttachments, true /* bExploreDependencies  */);
		}
		if (DiscoveredDependencies)
		{
			AddPlatformDependencyRange(*DiscoveredDependencies, PlatformIndex, EInstigator::HardDependency);
		}
	}
	if (PlatformDependencyMap.IsEmpty())
	{
		return;
	}

	TArray<FPackageData*>* Edges = nullptr;
	for (TPair<FName, FScratchPlatformDependencyBits>& PlatformDependencyPair : PlatformDependencyMap)
	{
		FName DependencyName = PlatformDependencyPair.Key;
		TBitArray<>& HasPlatformByIndex = PlatformDependencyPair.Value.HasPlatformByIndex;
		EInstigator InstigatorType = PlatformDependencyPair.Value.InstigatorType;

		// Process any CoreRedirects before checking whether the package exists
		FName Redirected = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package,
			FCoreRedirectObjectName(NAME_None, NAME_None, DependencyName)).PackageName;
		DependencyName = Redirected;

		FVertexData& DependencyVertex = FindOrAddVertex(DependencyName);
		if (!DependencyVertex.PackageData)
		{
			continue;
		}
		FPackageData& DependencyPackageData(*DependencyVertex.PackageData);
		bool bAddToFrontier = false;

		for (int32 PlatformIndex = 0; PlatformIndex < LocalNumFetchPlatforms; ++PlatformIndex)
		{
			if (!HasPlatformByIndex[PlatformIndex])
			{
				continue;
			}
			FFetchPlatformData& FetchPlatformData = FetchPlatforms[PlatformIndex];
			const ITargetPlatform* TargetPlatform = FetchPlatformData.Platform;
			FPackagePlatformData& PlatformData = DependencyPackageData.FindOrAddPlatformData(TargetPlatform);

			if (PlatformIndex == CookerLoadingPlatformIndex)
			{
				if (!Edges)
				{
					Edges = &GraphEdges.FindOrAdd(&PackageData);
					Edges->Reset(PlatformDependencyMap.Num());
				}
				Edges->Add(&DependencyPackageData);
			}

			if (!PlatformData.IsReachable())
			{
				PlatformData.SetReachable(true);
				if (!DependencyPackageData.HasInstigator() && TargetPlatform != CookerLoadingPlatformKey)
				{
					DependencyPackageData.SetInstigator(Cluster, FInstigator(InstigatorType, PackageName));
				}
			}
			if (!PlatformData.IsVisitedByCluster())
			{
				bAddToFrontier = true;
			}
		}
		if (bAddToFrontier)
		{
			AddToFrontier(DependencyVertex);
		}
	}
}

void FRequestCluster::FVertexQueryData::Reset()
{
	for (FQueryPlatformData& PlatformData : Platforms)
	{
		PlatformData.CookAttachments.Reset();
		PlatformData.bActive = false;
	}
}

FRequestCluster::FVertexData* FRequestCluster::FGraphSearch::AllocateVertex()
{
	return VertexAllocator.NewElement();
}

FRequestCluster::FVertexQueryData* FRequestCluster::FGraphSearch::AllocateQueryData()
{
	// VertexQueryAllocator uses DeferredDestruction, so this might be a resused Batch, but we don't need to Reset it
	// during allocation because Batches are Reset during Free.
	return VertexQueryAllocator.NewElement();
}

void FRequestCluster::FGraphSearch::FreeQueryData(FVertexQueryData* QueryData)
{
	QueryData->Reset();
	VertexQueryAllocator.Free(QueryData);
}

FRequestCluster::FVertexData&
FRequestCluster::FGraphSearch::FindOrAddVertex(FName PackageName)
{
	// Only called from PumpExploration thread
	FVertexData*& ExistingVertex = Vertices.FindOrAdd(PackageName);
	if (ExistingVertex)
	{
		return *ExistingVertex;
	}

	ExistingVertex = AllocateVertex();
	TStringBuilder<256> NameBuffer;
	PackageName.ToString(NameBuffer);
	ExistingVertex->PackageData = nullptr;
	if (!FPackageName::IsScriptPackage(NameBuffer))
	{
		ExistingVertex->PackageData = Cluster.COTFS.PackageDatas->TryAddPackageDataByPackageName(PackageName);
	}
	return *ExistingVertex;
}

FRequestCluster::FVertexData&
FRequestCluster::FGraphSearch::FindOrAddVertex(FName PackageName, FPackageData& PackageData)
{
	// Only called from PumpExploration thread
	FVertexData*& ExistingVertex = Vertices.FindOrAdd(PackageName);
	if (ExistingVertex)
	{
		check(ExistingVertex->PackageData == &PackageData);
		return *ExistingVertex;
	}

	ExistingVertex = AllocateVertex();
	ExistingVertex->PackageData = &PackageData;
	return *ExistingVertex;
}

void FRequestCluster::FGraphSearch::AddToFrontier(FVertexData& Vertex)
{
	if (Vertex.PackageData)
	{
		Cluster.PullIntoCluster(*Vertex.PackageData);
	}
	Frontier.Add(&Vertex);
}

void FRequestCluster::FGraphSearch::CreateAvailableBatches(bool bAllowIncompleteBatch)
{
	constexpr int32 BatchSize = 1000;
	if (PreAsyncQueue.IsEmpty() || (!bAllowIncompleteBatch && PreAsyncQueue.Num() < BatchSize))
	{
		return;
	}

	TArray<FQueryVertexBatch*> NewBatches;
	NewBatches.Reserve((PreAsyncQueue.Num() + BatchSize - 1) / BatchSize);
	{
		FScopeLock ScopeLock(&Lock);
		while (PreAsyncQueue.Num() >= BatchSize)
		{
			NewBatches.Add(CreateBatchOfPoppedVertices(BatchSize));
		}
		if (PreAsyncQueue.Num() > 0 && bAllowIncompleteBatch)
		{
			NewBatches.Add(CreateBatchOfPoppedVertices(PreAsyncQueue.Num()));
		}
	}
	for (FQueryVertexBatch* NewBatch : NewBatches)
	{
		NewBatch->Send();
	}
}

FRequestCluster::FQueryVertexBatch* FRequestCluster::FGraphSearch::AllocateBatch()
{
	// Called from inside this->Lock
	// BatchAllocator uses DeferredDestruction, so this might be a resused Batch, but we don't need to Reset it during
	// allocation because Batches are Reset during Free.
	return BatchAllocator.NewElement(*this);
}

void FRequestCluster::FGraphSearch::FreeBatch(FQueryVertexBatch* Batch)
{
	// Called from inside this->Lock
	Batch->Reset();
	BatchAllocator.Free(Batch);
}

FRequestCluster::FQueryVertexBatch* FRequestCluster::FGraphSearch::CreateBatchOfPoppedVertices(int32 BatchSize)
{
	// Called from inside this->Lock
	check(BatchSize <= PreAsyncQueue.Num());
	FQueryVertexBatch* BatchData = AllocateBatch();
	BatchData->Vertices.Reserve(BatchSize);
	for (int32 BatchIndex = 0; BatchIndex < BatchSize; ++BatchIndex)
	{
		FVertexData* Vertex = PreAsyncQueue.PopFrontValue();
		FVertexData*& ExistingVert = BatchData->Vertices.FindOrAdd(Vertex->QueryData->PackageName);
		check(!ExistingVert); // We should not have any duplicate names in PreAsyncQueue
		ExistingVert = Vertex;
	}
	AsyncQueueBatches.Add(BatchData);
	return BatchData;
}

void FRequestCluster::FGraphSearch::OnBatchCompleted(FQueryVertexBatch* Batch)
{
	FScopeLock ScopeLock(&Lock);
	AsyncQueueBatches.Remove(Batch);
	FreeBatch(Batch);
	AsyncResultsReadyEvent->Trigger();
}

void FRequestCluster::FGraphSearch::OnVertexCompleted()
{
	// The trigger occurs outside of the lock, and might get clobbered and incorrectly ignored by a call from the
	// consumer thread if the consumer tried to consume and found the vertices empty before our caller added a vertex
	// but then pauses and calls AsyncResultsReadyEvent->Reset after this AsyncResultsReadyEvent->Trigger.
	// This clobbering will not cause a deadlock, because eventually DestroyBatch will be called which triggers it
	// inside the lock. Doing the per-vertex trigger outside the lock is good for performance.
	AsyncResultsReadyEvent->Trigger();
}

FRequestCluster::FQueryVertexBatch::FQueryVertexBatch(FGraphSearch& InGraphSearch)
	: ThreadSafeOnlyVars(InGraphSearch)
{
	PlatformDatas.SetNum(InGraphSearch.FetchPlatforms.Num());
}

void FRequestCluster::FQueryVertexBatch::Reset()
{
	for (FPlatformData& PlatformData : PlatformDatas)
	{
		PlatformData.PackageNames.Reset();
	}
	Vertices.Reset();
}

void FRequestCluster::FQueryVertexBatch::Send()
{
	for (const TPair<FName, FVertexData*>& Pair : Vertices)
	{
		FVertexData* Vertex = Pair.Value;
		TArray<FQueryPlatformData>& QueryPlatforms = Vertex->QueryData->Platforms;
		bool bAtLeastOnePlatform = false;
		for (int32 PlatformIndex = 0; PlatformIndex < PlatformDatas.Num(); ++PlatformIndex)
		{
			if (QueryPlatforms[PlatformIndex].bActive)
			{
				PlatformDatas[PlatformIndex].PackageNames.Add(Pair.Key);
			}
			bAtLeastOnePlatform = true;
		}
		// We only check for the vertex's completion when the vertex receives a callback from the completion of a
		// platform. Therefore we do not support Vertices in the batch that have no platforms.
		check(bAtLeastOnePlatform);
	}
	PendingVertices.store(Vertices.Num(), std::memory_order_release);

	for (int32 PlatformIndex = 0; PlatformIndex < PlatformDatas.Num(); ++PlatformIndex)
	{
		FPlatformData& PlatformData = PlatformDatas[PlatformIndex];
		if (PlatformData.PackageNames.IsEmpty())
		{
			continue;
		}
		FFetchPlatformData& FetchPlatformData = ThreadSafeOnlyVars.FetchPlatforms[PlatformIndex];

		if (ThreadSafeOnlyVars.Cluster.IsIncrementalCook() // Only FetchCookAttachments if our cookmode supports it. Otherwise keep them all empty
			&& !FetchPlatformData.bIsPlatformAgnosticPlatform // The PlatformAgnosticPlatform has no stored CookAttachments; always use empty
			&& !FetchPlatformData.bIsCookerLoadingPlatform // The CookerLoadingPlatform has no stored CookAttachments; always use empty
			)
		{
			TUniqueFunction<void(FName PackageName, UE::TargetDomain::FCookAttachments&& Result)> Callback =
				[this, PlatformIndex](FName PackageName, UE::TargetDomain::FCookAttachments&& Attachments)
			{
				RecordCacheResults(PackageName, PlatformIndex, MoveTemp(Attachments));
			};
			UE::TargetDomain::FetchCookAttachments(PlatformData.PackageNames, FetchPlatformData.Platform,
				FetchPlatformData.Writer, MoveTemp(Callback));
		}
		else
		{
			// When we do not need to asynchronously fetch, we record empty cache results to keep the edgefetch
			// flow similar to the FetchCookAttachments case

			// Don't use a ranged-for, as we are not allowed to access this or this->PackageNames after the
			// last index, and ranged-for != at the end of the final loop iteration can read from PackageNames
			int32 NumPackageNames = PlatformData.PackageNames.Num();
			FName* PackageNamesData = PlatformData.PackageNames.GetData();
			for (int32 PackageNameIndex = 0; PackageNameIndex < NumPackageNames; ++PackageNameIndex)
			{
				FName PackageName = PackageNamesData[PackageNameIndex];
				UE::TargetDomain::FCookAttachments Attachments;
				RecordCacheResults(PackageName, PlatformIndex, MoveTemp(Attachments));
			}
		}
	}
}

void FRequestCluster::FQueryVertexBatch::RecordCacheResults(FName PackageName, int32 PlatformIndex,
	UE::TargetDomain::FCookAttachments&& CookAttachments)
{
	FVertexData* Vertex = Vertices.FindChecked(PackageName);
	check(Vertex->QueryData);
	FVertexQueryData& QueryData = *Vertex->QueryData;
	QueryData.Platforms[PlatformIndex].CookAttachments = MoveTemp(CookAttachments);
	if (QueryData.PendingPlatforms.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		ThreadSafeOnlyVars.AsyncQueueResults.Enqueue(Vertex);
		bool bBatchComplete = PendingVertices.fetch_sub(1, std::memory_order_relaxed) == 1;
		if (!bBatchComplete)
		{
			ThreadSafeOnlyVars.OnVertexCompleted();
		}
		else
		{
			ThreadSafeOnlyVars.OnBatchCompleted(this);
			// *this is no longer accessible
		}
	}
}

TMap<FPackageData*, TArray<FPackageData*>>& FRequestCluster::FGraphSearch::GetGraphEdges()
{
	return GraphEdges;
}

bool FRequestCluster::IsIncrementalCook() const
{
	return bAllowIterativeResults && COTFS.bHybridIterativeEnabled;
}

void FRequestCluster::IsRequestCookable(const ITargetPlatform* Platform, FPackageData& PackageData,
	UCookOnTheFlyServer& COTFS, ESuppressCookReason& OutReason, bool& bOutCookable, bool& bOutExplorable)
{
	FString LocalDLCPath;
	if (COTFS.CookByTheBookOptions->bErrorOnEngineContentUse)
	{
		LocalDLCPath = FPaths::Combine(*COTFS.GetBaseDirectoryForDLC(), TEXT("Content"));
		FPaths::MakeStandardFilename(LocalDLCPath);
	}

	IsRequestCookable(Platform, PackageData.GetPackageName(), PackageData, COTFS,
		LocalDLCPath, OutReason, bOutCookable, bOutExplorable);
}

void FRequestCluster::IsRequestCookable(const ITargetPlatform* Platform, FName PackageName, FPackageData& PackageData,
	ESuppressCookReason& OutReason, bool& bOutCookable, bool& bOutExplorable)
{
	return IsRequestCookable(Platform, PackageName, PackageData, COTFS,
		DLCPath, OutReason, bOutCookable, bOutExplorable);
}

void FRequestCluster::IsRequestCookable(const ITargetPlatform* Platform, FName PackageName, FPackageData& PackageData,
	UCookOnTheFlyServer& InCOTFS, FStringView InDLCPath, ESuppressCookReason& OutReason, bool& bOutCookable, bool& bOutExplorable)
{
	check(Platform != CookerLoadingPlatformKey); // IsRequestCookable should not be called for The CookerLoadingPlatform; it has different rules

	TStringBuilder<256> NameBuffer;
	// We need to reject packagenames from adding themselves or their transitive dependencies using all the same rules that
	// UCookOnTheFlyServer::ProcessRequest uses. Packages that are rejected from cook do not add their dependencies to the cook.
	PackageName.ToString(NameBuffer);
	if (FPackageName::IsScriptPackage(NameBuffer))
	{
		OutReason = ESuppressCookReason::ScriptPackage;
		bOutCookable = false;
		bOutExplorable = false;
		return;
	}

	FPackagePlatformData* PlatformData = PackageData.FindPlatformData(Platform);
	bool bExplorableOverride = PlatformData ? PlatformData->IsExplorableOverride() : false;
	ON_SCOPE_EXIT
	{
		bOutExplorable = bOutExplorable | bExplorableOverride;
	};

	FName FileName = PackageData.GetFileName();
	if (InCOTFS.PackageTracker->NeverCookPackageList.Contains(FileName))
	{
		if (INDEX_NONE != UE::String::FindFirst(NameBuffer, ULevel::GetExternalActorsFolderName(), ESearchCase::IgnoreCase))
		{
			// EXTERNALACTOR_TODO: Add a separate category for ExternalActors rather than putting them in
			// NeverCookPackageList and checking naming convention here.
			OutReason = ESuppressCookReason::NeverCook;
			bOutCookable = false;

			// EXTERNALACTOR_TODO: We want to explore externalactors, because they add references to the cook that will
			// otherwise not be found until the map package loads them and adds them as unsolicited packages
			// But some externalactor packages will never be loaded by the generator, and we don't have a way to discover which
			// ones will not be loaded until we load the Map and WorldPartition object.
			// So set them to explorable = false until we implement an interface to determine which actors will be loaded up front.
			bOutExplorable = false;
		}
		else
		{
			UE_LOG(LogCook, Verbose, TEXT("Package %s is referenced but is in the never cook package list, discarding request"), *NameBuffer);
			OutReason = ESuppressCookReason::NeverCook;
			bOutCookable = false;
			bOutExplorable = false;
		}
		return;
	}

	if (InCOTFS.CookByTheBookOptions->bErrorOnEngineContentUse && !InDLCPath.IsEmpty())
	{
		FileName.ToString(NameBuffer);
		if (!FStringView(NameBuffer).StartsWith(InDLCPath))
		{
			// Editoronly content that was not cooked by the base game is allowed to be "cooked"; if it references
			// something not editoronly then we will exclude and give a warning on that followup asset. We need to
			// handle editoronly objects being referenced because the base game will not have marked them as cooked so
			// we will think we still need to "cook" them.
			// The only case where this comes up is in ObjectRedirectors, so we only test for those for performance.
			TArray<FAssetData> Assets;
			IAssetRegistry::GetChecked().GetAssetsByPackageName(PackageName, Assets,
				true /* bIncludeOnlyOnDiskAssets */);
			bool bEditorOnly = !Assets.IsEmpty() &&
				Algo::AllOf(Assets, [](const FAssetData& Asset)
					{
						return Asset.IsRedirector();
					});

			if (!bEditorOnly)
			{
				if (!PackageData.HasCookedPlatform(Platform, true /* bIncludeFailed */))
				{
					// AllowUncookedAssetReferences should only be used when the DLC plugin to cook is going to be mounted where uncooked packages are available.
					// This will allow a DLC plugin to be recooked continually and mounted in an uncooked editor which is useful for CI.
					if (!InCOTFS.CookByTheBookOptions->bAllowUncookedAssetReferences)
					{
						UE_LOG(LogCook, Error, TEXT("Uncooked Engine or Game content %s is being referenced by DLC!"), *NameBuffer);
					}
				}
				OutReason = ESuppressCookReason::NotInCurrentPlugin;
				bOutCookable = false;
				bOutExplorable = false;
				return;
			}
		}
	}

	// The package is ordinarily cookable and explorable. In some cases we filter out for testing
	// packages that are ordinarily cookable; set bOutCookable to false if so.
	bOutExplorable = true;
	if (InCOTFS.bCookFilter)
	{
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		if (!InCOTFS.CookFilterIncludedClasses.IsEmpty())
		{
			TOptional<FAssetPackageData> AssetData = AssetRegistry.GetAssetPackageDataCopy(PackageName);
			bool bIncluded = false;
			if (AssetData)
			{
				for (FName ClassName : AssetData->ImportedClasses)
				{
					if (InCOTFS.CookFilterIncludedClasses.Contains(ClassName))
					{
						bIncluded = true;
						break;
					}
				}
			}
			if (!bIncluded)
			{
				OutReason = ESuppressCookReason::CookFilter;
				bOutCookable = false;
				return;
			}
		}
		if (!InCOTFS.CookFilterIncludedAssetClasses.IsEmpty())
		{
			TArray<FAssetData> AssetDatas;
			AssetRegistry.GetAssetsByPackageName(PackageName, AssetDatas, true /* bIncludeOnlyDiskAssets */);
			bool bIncluded = false;
			for (FAssetData& AssetData : AssetDatas)
			{
				if (InCOTFS.CookFilterIncludedAssetClasses.Contains(FName(*AssetData.AssetClassPath.ToString())))
				{
					bIncluded = true;
					break;
				}
			}
			if (!bIncluded)
			{
				OutReason = ESuppressCookReason::CookFilter;
				bOutCookable = false;
				return;
			}
		}
	}

	OutReason = ESuppressCookReason::NotSuppressed;
	bOutCookable = true;
}

TConstArrayView<FName> FRequestCluster::GetLocalizationReferences(FName PackageName, UCookOnTheFlyServer& InCOTFS)
{
	if (!FPackageName::IsLocalizedPackage(WriteToString<256>(PackageName)))
	{
		TArray<FName>* Result = InCOTFS.CookByTheBookOptions->SourceToLocalizedPackageVariants.Find(PackageName);
		if (Result)
		{
			return TConstArrayView<FName>(*Result);
		}
	}
	return TConstArrayView<FName>();
}

TArray<FName> FRequestCluster::GetAssetManagerReferences(FName PackageName)
{
	TArray<FName> Results;
	UAssetManager::Get().ModifyCookReferences(PackageName, Results);
	return Results;
}

template <typename T>
static void ArrayShuffle(TArray<T>& Array)
{
	// iterate 0 to N-1, picking a random remaining vertex each loop
	int32 N = Array.Num();
	for (int32 I = 0; I < N; ++I)
	{
		Array.Swap(I, FMath::RandRange(I, N - 1));
	}
}

template <typename T>
static TArray<T> FindRootsFromLeafToRootOrderList(TConstArrayView<T> LeafToRootOrder, const TMap<T, TArray<T>>& Edges,
	const TSet<T>& ValidVertices)
{
	// Iteratively
	//    1) Add the leading rootward non-visited element to the root
	//    2) Visit all elements reachable from that root
	// This works because the input array is already sorted RootToLeaf, so we
	// know the leading element has no incoming edges from anything later.
	TArray<T> Roots;
	TSet<T> Visited;
	Visited.Reserve(LeafToRootOrder.Num());
	struct FVisitEntry
	{
		T Vertex;
		const TArray<T>* Edges;
		int32 NextEdge;
		void Set(T V, const TMap<T, TArray<T>>& AllEdges)
		{
			Vertex = V;
			Edges = AllEdges.Find(V);
			NextEdge = 0;
		}
	};
	TArray<FVisitEntry> DFSStack;
	int32 StackNum = 0;
	auto Push = [&DFSStack, &Edges, &StackNum](T Vertex)
	{
		while (DFSStack.Num() <= StackNum)
		{
			DFSStack.Emplace();
		}
		DFSStack[StackNum++].Set(Vertex, Edges);
	};
	auto Pop = [&StackNum]()
	{
		--StackNum;
	};

	for (T Root : ReverseIterate(LeafToRootOrder))
	{
		bool bAlreadyExists;
		Visited.Add(Root, &bAlreadyExists);
		if (bAlreadyExists)
		{
			continue;
		}
		Roots.Add(Root);

		Push(Root);
		check(StackNum == 1);
		while (StackNum > 0)
		{
			FVisitEntry& Entry = DFSStack[StackNum - 1];
			bool bPushed = false;
			while (Entry.Edges && Entry.NextEdge < Entry.Edges->Num())
			{
				T Target = (*Entry.Edges)[Entry.NextEdge++];
				Visited.Add(Target, &bAlreadyExists);
				if (!bAlreadyExists && ValidVertices.Contains(Target))
				{
					Push(Target);
					bPushed = true;
					break;
				}
			}
			if (!bPushed)
			{
				Pop();
			}
		}
	}
	return Roots;
}

void FRequestCluster::RandomizeCookOrder(TArray<FPackageData*>& InOutLeafToRootOrder,
	const TMap<FPackageData*, TArray<FPackageData*>>& Edges)
{
	// Notes on the ideal solution:
	// In a graph without cycles, the visitation order of a DepthFirstSearch starting from the graph roots is a
	// RootToLeaf ordering. We can randomize by randomly iterating the graph roots and by randomly iterating the
	// edges when moving from each graph.
	// In a graph with cycles, a RootToLeaf order is only defined in the condensed graph, and for
	// each chain within a node of the condensed graph, we can randomize the vertices in the chain.
	// But this requires creating the condensed graph, which is expensive.
	// 
	// Notes on the practical solution:
	// Cycles are not supposed to be a large part of our graph, so we will not seek to do a good job of randomizing
	// them. We do a DFS as we would in an acyclic graph, and when we encounter an already-visited vertex due to a cycle
	// we just pretend that edge does not exist.
	//
	// Two DFS passes
	// Pass 1, find all roots by iterating from root to leaf and DFSing each remaining head element.
	// Pass 2: Iterate all roots in random order and DFS each one; append the leaf-to-root order of their search
	// to the final leaf-to-root order.
	if (InOutLeafToRootOrder.IsEmpty())
	{
		return;
	}

	struct FVisitEntry
	{
		FPackageData* Vertex;
		TArray<FPackageData*> Edges;
		int32 NextEdge;
		void Set(FPackageData* V, const TMap<FPackageData*,TArray<FPackageData*>>& AllEdges)
		{
			Vertex = V;
			Edges.Reset();
			const TArray<FPackageData*>* EdgesFromV = AllEdges.Find(V);
			if (EdgesFromV)
			{
				Edges.Append(*EdgesFromV);
				ArrayShuffle(Edges);
			}
			NextEdge = 0;
		}
	};
	
	TArray<FVisitEntry> DFSStack;
	int32 StackNum = 0;
	auto Push = [&DFSStack, &Edges, &StackNum](FPackageData* Vertex)
	{
		while (DFSStack.Num() <= StackNum)
		{
			DFSStack.Emplace();
		}
		DFSStack[StackNum++].Set(Vertex, Edges);
	};
	auto Pop = [&StackNum]()
	{
		--StackNum;
	};

	TSet<FPackageData*> ValidVertices;
	ValidVertices.Append(InOutLeafToRootOrder);

	TArray<FPackageData*> Roots = FindRootsFromLeafToRootOrderList<FPackageData*>(InOutLeafToRootOrder, Edges, ValidVertices);
	ArrayShuffle(Roots);

	TSet<FPackageData*> Visited;
	Visited.Reserve(InOutLeafToRootOrder.Num());
	TArray<FPackageData*> OutputVisitOrder;

	for (FPackageData* Root : Roots)
	{
		bool bAlreadyExists;
		Visited.Add(Root, &bAlreadyExists);
		if (bAlreadyExists)
		{
			continue;
		}

		Push(Root);
		check(StackNum == 1);
		while (StackNum > 0)
		{
			FVisitEntry& Entry = DFSStack[StackNum - 1];
			bool bPushed = false;
			while (Entry.NextEdge < Entry.Edges.Num())
			{
				FPackageData* Target = Entry.Edges[Entry.NextEdge++];
				Visited.Add(Target, &bAlreadyExists);
				if (!bAlreadyExists && ValidVertices.Contains(Target))
				{
					Push(Target);
					bPushed = true;
					break;
				}
			}
			if (!bPushed)
			{
				// Add this vertex now as the most rootwards encountered; it is farther toward the root than any of the vertices it depends on
				OutputVisitOrder.Add(Entry.Vertex);
				Pop();
			}
		}
	}
	Visited.Empty();
	Roots.Empty();

	TMap<FPackageData*, int32> OriginalIndices;
	OriginalIndices.Reserve(InOutLeafToRootOrder.Num());
	for (int32 Index = 0; Index < InOutLeafToRootOrder.Num(); ++Index)
	{
		OriginalIndices.Add(InOutLeafToRootOrder[Index], Index);
	}
	check(OriginalIndices.Num() == InOutLeafToRootOrder.Num());
	
	// Copy the OutputVisitOrder over top of our InOut ordered parameter and report the diagnostic
	// for the average shuffle distance.

	double SumSquaredDistances = 0.0;

	int32 WriteIndex = 0;
	for (FPackageData* Vertex : OutputVisitOrder)
	{
		int32 OriginalIndex;
		verify(OriginalIndices.RemoveAndCopyValue(Vertex, OriginalIndex));
		double Distance = static_cast<double>(OriginalIndex - WriteIndex);
		SumSquaredDistances += FMath::Square(Distance);

		check(WriteIndex < InOutLeafToRootOrder.Num());
		InOutLeafToRootOrder[WriteIndex++] = Vertex;
	}
	check(WriteIndex == InOutLeafToRootOrder.Num());

	UE_LOG(LogCook, Display,
		TEXT("RandomPackageOrder used, packages in the cluster were shuffled. %d elements in cluster, average shuffle distance == %0.1f."),
		InOutLeafToRootOrder.Num(), (float) FMath::Sqrt(SumSquaredDistances/InOutLeafToRootOrder.Num()));
}

} // namespace UE::Cook
