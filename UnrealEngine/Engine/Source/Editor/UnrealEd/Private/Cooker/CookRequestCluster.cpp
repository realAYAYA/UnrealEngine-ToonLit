// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/CookRequestCluster.h"

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
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/RedirectCollector.h"
#include "Misc/StringBuilder.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SavePackage.h"

namespace UE::Cook
{

FRequestCluster::FRequestCluster(UCookOnTheFlyServer& InCOTFS, TArray<const ITargetPlatform*>&& InPlatforms)
	: Platforms(MoveTemp(InPlatforms))
	, COTFS(InCOTFS)
	, PackageDatas(*InCOTFS.PackageDatas)
	, AssetRegistry(*IAssetRegistry::Get())
	, PackageTracker(*InCOTFS.PackageTracker)
	, BuildDefinitions(*InCOTFS.BuildDefinitions)
{
}

FRequestCluster::FRequestCluster(UCookOnTheFlyServer& InCOTFS, TConstArrayView<const ITargetPlatform*> InPlatforms)
	: Platforms(InPlatforms)
	, COTFS(InCOTFS)
	, PackageDatas(*InCOTFS.PackageDatas)
	, AssetRegistry(*IAssetRegistry::Get())
	, PackageTracker(*InCOTFS.PackageTracker)
	, BuildDefinitions(*InCOTFS.BuildDefinitions)
{
}

void FRequestCluster::AddClusters(UCookOnTheFlyServer& InCOTFS, TArray<FFilePlatformRequest>&& InRequests,
	bool bRequestsAreUrgent, TRingBuffer<FRequestCluster>& OutClusters)
{
	if (InRequests.Num() == 0)
	{
		return;
	}

	TArray<FRequestCluster, TInlineAllocator<1>> AddedClusters;
	auto FindOrAddClusterForPlatforms =
		[&AddedClusters, &InCOTFS](TArray<const ITargetPlatform*>&& InPlatforms)
	{
		for (FRequestCluster& Existing : AddedClusters)
		{
			if (Existing.GetPlatforms() == InPlatforms)
			{
				InPlatforms.Reset();
				return &Existing;
			}
		}
		return &AddedClusters.Emplace_GetRef(InCOTFS, MoveTemp(InPlatforms));
	};

	UE::Cook::FRequestCluster* MRUCluster = FindOrAddClusterForPlatforms(MoveTemp(InRequests[0].GetPlatforms()));
	// The usual case is all platforms are the same, so reserve the first Cluster's size assuming it will get all requests
	MRUCluster->InRequests.Reserve(InRequests.Num());
	// Add the first Request to it. Since we've already taken away the Platforms from the first request, we have to handle it specially.
	MRUCluster->InRequests.Add(FFileNameRequest(MoveTemp(InRequests[0]), bRequestsAreUrgent));

	for (FFilePlatformRequest& Request : TArrayView<FFilePlatformRequest>(InRequests).Slice(1, InRequests.Num() - 1))
	{
		if (Request.GetPlatforms() != MRUCluster->GetPlatforms())
		{
			// MRUCluster points to data inside AddedClusters, so we have to recalculate MRUCluster whenever we add
			MRUCluster = FindOrAddClusterForPlatforms(MoveTemp(Request.GetPlatforms()));
		}
		else
		{
			Request.GetPlatforms().Reset();
		}

		MRUCluster->InRequests.Add(FFileNameRequest(MoveTemp(Request), bRequestsAreUrgent));
	}

	for (UE::Cook::FRequestCluster& AddedCluster : AddedClusters)
	{
		AddedCluster.Initialize(InCOTFS);
		OutClusters.Add(MoveTemp(AddedCluster));
	}
}

FName GInstigatorRequestCluster(TEXT("RequestCluster"));

void FRequestCluster::AddClusters(UCookOnTheFlyServer& InCOTFS, FPackageDataSet& UnclusteredRequests,
	TRingBuffer<FRequestCluster>& OutClusters, FRequestQueue& QueueForReadyRequests)
{
	if (UnclusteredRequests.Num() == 0)
	{
		return;
	}

	TArray<FRequestCluster, TInlineAllocator<1>> AddedClusters;
	TArray<const ITargetPlatform*, TInlineAllocator<ExpectedMaxNumPlatforms>> RequestedPlatforms;
	auto FindOrAddClusterForPlatforms = [&AddedClusters, &InCOTFS, &RequestedPlatforms, &UnclusteredRequests]()
	{
		if (AddedClusters.Num() == 0)
		{
			FRequestCluster& Cluster = AddedClusters.Emplace_GetRef(InCOTFS, RequestedPlatforms);
			// The usual case is all platforms are the same, so reserve the first Cluster's size assuming it will get all requests
			Cluster.Requests.Reserve(UnclusteredRequests.Num());
			return &Cluster;
		}
		for (FRequestCluster& Existing : AddedClusters)
		{
			if (Existing.GetPlatforms() == RequestedPlatforms)
			{
				return &Existing;
			}
		}
		return &AddedClusters.Emplace_GetRef(InCOTFS, RequestedPlatforms);
	};

	bool bErrorOnEngineContentUse = false;
	bool bAllowUncookedAssetReferences = false;
	UE::Cook::FCookByTheBookOptions& Options = *InCOTFS.CookByTheBookOptions;
	FString DLCPath;
	bErrorOnEngineContentUse = Options.bErrorOnEngineContentUse;
	bAllowUncookedAssetReferences = Options.bAllowUncookedAssetReferences;
	if (bErrorOnEngineContentUse)
	{
		DLCPath = FPaths::Combine(*InCOTFS.GetBaseDirectoryForDLC(), TEXT("Content"));
		FPaths::MakeStandardFilename(DLCPath);
	}

	UE::Cook::FRequestCluster* MRUCluster = nullptr;
	for (FPackageData* PackageData : UnclusteredRequests)
	{
		if (PackageData->AreAllRequestedPlatformsExplored())
		{
			QueueForReadyRequests.AddReadyRequest(PackageData);
			continue;
		}

		// For non-cookable packages that we skipped in an earlier cluster but loaded because
		// they are hard dependencies, avoid the work of creating a cluster just for them,
		// by checking non-cookable and sending the package to idle instead of adding to
		// a cluster
		PackageData->GetRequestedPlatforms(RequestedPlatforms);
		ESuppressCookReason SuppressCookReason;
		if (!IsRequestCookable(PackageData->GetPackageName(), PackageData, *InCOTFS.PackageDatas, *InCOTFS.PackageTracker,
			DLCPath, bErrorOnEngineContentUse, bAllowUncookedAssetReferences, RequestedPlatforms, SuppressCookReason))
		{
			InCOTFS.DemoteToIdle(*PackageData, ESendFlags::QueueAdd, SuppressCookReason);
			continue;
		}

		if (!MRUCluster || RequestedPlatforms != MRUCluster->GetPlatforms())
		{
			// MRUCluster points to data inside AddedClusters, so we have to recalculate MRUCluster whenever we add
			MRUCluster = FindOrAddClusterForPlatforms();
		}
		CA_ASSUME(MRUCluster);

		MRUCluster->Requests.Add(PackageData);
	}

	for (UE::Cook::FRequestCluster& AddedCluster : AddedClusters)
	{
		AddedCluster.Initialize(InCOTFS);
		OutClusters.Add(MoveTemp(AddedCluster));
	}
}

FRequestCluster::FFileNameRequest::FFileNameRequest(FFilePlatformRequest&& FileRequest, bool bInUrgent)
	: FileName(FileRequest.GetFilename())
	, Instigator(MoveTemp(FileRequest.GetInstigator()))
	, CompletionCallback(MoveTemp(FileRequest.GetCompletionCallback()))
	, bUrgent(bInUrgent)
{
}

void FRequestCluster::Initialize(UCookOnTheFlyServer& InCOTFS)
{
	if (!COTFS.IsCookOnTheFlyMode())
	{
		UE::Cook::FCookByTheBookOptions& Options = *COTFS.CookByTheBookOptions;
		bAllowHardDependencies = !Options.bSkipHardReferences;
		bAllowSoftDependencies = !Options.bSkipSoftReferences;
		bErrorOnEngineContentUse = Options.bErrorOnEngineContentUse;
		bAllowUncookedAssetReferences = Options.bAllowUncookedAssetReferences;
	}
	else
	{
		// Do not queue soft-dependencies during CookOnTheFly; wait for them to be requested
		// TODO: Report soft dependencies separately, and mark them as normal priority,
		// and mark all hard dependencies as high priority in cook on the fly.
		bAllowSoftDependencies = false;
	}
	bHybridIterativeEnabled = COTFS.bHybridIterativeEnabled;
	if (bErrorOnEngineContentUse)
	{
		DLCPath = FPaths::Combine(*COTFS.GetBaseDirectoryForDLC(), TEXT("Content"));
		FPaths::MakeStandardFilename(DLCPath);
	}
	GConfig->GetBool(TEXT("CookSettings"), TEXT("PreQueueBuildDefinitions"), bPreQueueBuildDefinitions, GEditorIni);


	PackageWriters.Reserve(Platforms.Num());
	bFullBuild = false;
	bool bFirst = true;
	for (const ITargetPlatform* TargetPlatform : Platforms)
	{
		PackageWriters.Add(&COTFS.FindOrCreatePackageWriter(TargetPlatform));
		FPlatformData* PlatformData = COTFS.PlatformManager->GetPlatformData(TargetPlatform);
		if (bFirst)
		{
			bFullBuild = PlatformData->bFullBuild;
			bFirst = false;
		}
		else
		{
			if (PlatformData->bFullBuild != bFullBuild)
			{
				UE_LOG(LogCook, Warning, TEXT("Full build is requested for some platforms but not others, but this is not supported. All platforms will be built full."));
				bFullBuild = true;
			}
		}
	}

	bool bRemoveNulls = false;
	for (FPackageData*& PackageData : Requests)
	{
		bool bAlreadyExists = false;
		OwnedPackageDatas.Add(PackageData, &bAlreadyExists);
		if (bAlreadyExists)
		{
			bRemoveNulls = true;
			PackageData = nullptr;
		}
	}
	if (bRemoveNulls)
	{
		Requests.Remove(nullptr);
	}
}

void FRequestCluster::Process(const FCookerTimer& CookerTimer, bool& bOutComplete)
{
	bOutComplete = true;

	FetchPackageNames(CookerTimer, bOutComplete);
	if (!bOutComplete)
	{
		return;
	}
	FetchDependencies(CookerTimer, bOutComplete);
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
	int32 InRequestsNum = InRequests.Num();
	if (this->NextRequest == 0)
	{
		Requests.Reserve(Requests.Num() + InRequestsNum);
	}
	for (;NextRequest < InRequestsNum; ++NextRequest)
	{
		if ((NextRequest+1) % TimerCheckPeriod == 0 && CookerTimer.IsTimeUp())
		{
			break;
		}

		FFileNameRequest& Request = InRequests[NextRequest];
		FName OriginalName = Request.FileName;
#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Processing request for package %s"), *OriginalName.ToString());
#endif
		// The input filenames are normalized, but might be missing their extension, so allow PackageDatas
		// to correct the filename if the package is found with a different filename
		bool bExactMatchRequired = false;
		FPackageData* PackageData = PackageDatas.TryAddPackageDataByStandardFileName(OriginalName, bExactMatchRequired,
			&Request.FileName);
		if (!PackageData)
		{
			LogCookerMessage(FString::Printf(TEXT("Could not find package at file %s!"),
				*OriginalName.ToString()), EMessageSeverity::Error);
			UE_LOG(LogCook, Error, TEXT("Could not find package at file %s!"), *OriginalName.ToString());
			UE::Cook::FCompletionCallback CompletionCallback(MoveTemp(Request.CompletionCallback));
			if (CompletionCallback)
			{
				CompletionCallback(nullptr);
			}
		}
		else
		{
			if (TryTakeOwnership(*PackageData, Request.bUrgent, MoveTemp(Request.CompletionCallback), Request.Instigator))
			{
				bool bAlreadyExists;
				OwnedPackageDatas.Add(PackageData, &bAlreadyExists);
				if (!bAlreadyExists)
				{
					Requests.Add(PackageData);
				}
			}
		}
	}
	if (NextRequest < InRequestsNum)
	{
		bOutComplete = false;
		return;
	}

	InRequests.Empty();
	NextRequest = 0;
	bPackageNamesComplete = true;
}

bool FRequestCluster::TryTakeOwnership(FPackageData& PackageData, bool bUrgent, UE::Cook::FCompletionCallback && CompletionCallback,
	const FInstigator& InInstigator)
{
	if (!PackageData.IsInProgress())
	{
		check(!COTFS.IsCookWorkerMode()); // CookWorkers skip dependency traversal and should only call TryTakeOwnership on incoming requests, which are already in progress
		check(GetPlatforms().Num() != 0); // This is required for SetRequestData
		if (PackageData.HasAllExploredPlatforms(GetPlatforms()))
		{
			// Leave it in idle - it's already been processed by a cluster
			if (CompletionCallback)
			{
				CompletionCallback(&PackageData);
			}
			return false;
		}
		else
		{
			PackageData.SetRequestData(GetPlatforms(), bUrgent, MoveTemp(CompletionCallback), FInstigator(InInstigator));
			PackageData.SendToState(EPackageState::Request, ESendFlags::QueueRemove);
			return true;
		}
	}
	else
	{
		if (PackageData.HasAllExploredPlatforms(GetPlatforms()))
		{
			// Leave it where it is - it's already been processed by a cluster - but update with our request data
			// This might demote it back to Request, and add it to Normal or Urgent request, but that will not
			// impact this RequestCluster
			PackageData.UpdateRequestData(GetPlatforms(), bUrgent, MoveTemp(CompletionCallback), FInstigator(InInstigator));
			return false;
		}
		else
		{
			if (!OwnedPackageDatas.Contains(&PackageData))
			{
				// Steal it from wherever it is and add it to this cluster
				// This might steal it from another RequestCluster or from the UnclusteredRequests if it's in request
				// Doing that steal is wasteful but okay; one of the RequestClusters will win it and keep it
				PackageData.SendToState(EPackageState::Request, ESendFlags::QueueRemove);
				PackageData.UpdateRequestData(GetPlatforms(), bUrgent, MoveTemp(CompletionCallback), FInstigator(InInstigator), false /* bAllowUpdateUrgency */);
			}
			return true;
		}
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
	if (EditorDomain)
	{
		bool bBatchDownloadEnabled = true;
		GConfig->GetBool(TEXT("EditorDomain"), TEXT("BatchDownloadEnabled"), bBatchDownloadEnabled, GEditorIni);
		if (bBatchDownloadEnabled)
		{
			// If the EditorDomain is active, then batch-download all packages to cook from remote cache into local
			TArray<FName> BatchDownload;
			BatchDownload.Reserve(Requests.Num());
			for (FPackageData* PackageData : Requests)
			{
				BatchDownload.Add(PackageData->GetPackageName());
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
		GraphSearch->GetTransitiveRequests().Remove(PackageData);
		TMap<FPackageData*, TArray<FPackageData*>>& GraphEdges = GraphSearch->GetGraphEdges();
		GraphEdges.Remove(PackageData);
		for (TPair<FPackageData*, TArray<FPackageData*>>& Pair : GraphEdges)
		{
			Pair.Value.Remove(PackageData);
		}
	}
	Requests.Remove(PackageData);
	RequestsToDemote.RemoveAll([PackageData](const TPair<FPackageData*, ESuppressCookReason>& Pair) { return Pair.Key == PackageData; });
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
		OutRequestsToLoad = MoveTemp(Requests);
		OutRequestsToDemote = MoveTemp(RequestsToDemote);
		OutRequestGraph = MoveTemp(RequestGraph);
		check(OutRequestsToLoad.Num() + OutRequestsToDemote.Num() == OwnedPackageDatas.Num())
	}
	else
	{
		OutRequestsToLoad = OwnedPackageDatas.Array();
		OutRequestsToDemote.Reset();
		OutRequestGraph.Reset();
	}
	InRequests.Empty();
	Requests.Empty();
	RequestsToDemote.Empty();
	OwnedPackageDatas.Empty();
	GraphSearch.Reset();
	RequestGraph.Reset();
	NextRequest = 0;
}

void FRequestCluster::FetchDependencies(const FCookerTimer& CookerTimer, bool& bOutComplete)
{
	if (bDependenciesComplete)
	{
		return;
	}

	if (COTFS.IsCookWorkerMode())
	{
		bDependenciesComplete = true;
		return;
	}

	if (!GraphSearch)
	{
		GraphSearch.Reset(new FGraphSearch(*this));
	}

	if (!bAllowHardDependencies)
	{
		// FetchDependencies is responsible for marking all requests as explored and demoting
		// the ones that are not cookable. If we're skipping the dependencies search, handle that
		// responsibility for the initial requests and return.
		for (FPackageData* PackageData : Requests)
		{
			GraphSearch->VisitVertex(FVertexData(FVertexData::ESkipDependencies, *PackageData, *this));
		}
		Swap(Requests, GraphSearch->GetTransitiveRequests());
		GraphSearch.Reset();
		bDependenciesComplete = true;
		return;
	}

	constexpr double WaitTime = 0.50;
	for (;;)
	{
		bool bIsDone;
		TArray<TUniquePtr<FVertexData>> CompletedVertices;
		GraphSearch->Poll(CompletedVertices, bIsDone);
		if (bIsDone)
		{
			break;
		}
		if (CompletedVertices.Num())
		{
			FHasPendingVerticesScope HasPendingVerticesScope(*GraphSearch);
			for (TUniquePtr<FVertexData>& VertexData : CompletedVertices)
			{
				// The PackageData may be removed from this due to external requests while the VertexData was processing;
				// discard the vertex now if we no longer own the PackageData.
				if (OwnedPackageDatas.Contains(VertexData->PackageData))
				{
					GraphSearch->VisitVertex(*VertexData);
				}
				GraphSearch->FreeVertex(MoveTemp(VertexData));
			}
		}
		else
		{
			GraphSearch->UpdateDisplay();
			GraphSearch->WaitForPollAvailability(WaitTime);
		}
		if (CookerTimer.IsTimeUp())
		{
			bOutComplete = false;
			return;
		}
	}

	TArray<FPackageData*>& NewRequests = GraphSearch->GetTransitiveRequests();
	check(NewRequests.Num() + RequestsToDemote.Num() == OwnedPackageDatas.Num());
	COOK_STAT(DetailedCookStats::NumPreloadedDependencies += FMath::Max(0, NewRequests.Num() - Requests.Num()));

	// Sort the NewRequests in leaf to root order and replace the requests list with NewRequests
	TArray<FPackageData*> Empty;
	auto GetElementDependencies = [this, &Empty](FPackageData* PackageData) -> const TArray<FPackageData*>&
	{
		const TArray<FPackageData*>* VertexEdges = GraphSearch->GetGraphEdges().Find(PackageData);
		return VertexEdges ? *VertexEdges : Empty;
	};
	Algo::TopologicalSort(NewRequests, GetElementDependencies, Algo::ETopologicalSort::AllowCycles);
	Requests = MoveTemp(NewRequests);

	RequestGraph = MoveTemp(GraphSearch->GetGraphEdges());
	GraphSearch.Reset();
	bDependenciesComplete = true;
}

FRequestCluster::FGraphSearch::FGraphSearch(FRequestCluster& InCluster)
	: Cluster(InCluster)
	, PollReadyEvent(EEventMode::ManualReset)
{
	PollReadyEvent->Trigger();
	bCookAttachmentsEnabled = !Cluster.bFullBuild && Cluster.bHybridIterativeEnabled;

	TArray<TUniquePtr<FVertexData>> Vertices;
	Vertices.Reserve(Cluster.Requests.Num());
	for (FPackageData* PackageData : Cluster.Requests)
	{
		TUniquePtr<FVertexData>& VertexData = Vertices.Emplace_GetRef();
		FindOrAddVertex(PackageData->GetPackageName(), PackageData, true /* bInitialRequest */,
			FInstigator(EInstigator::Unspecified, GInstigatorRequestCluster), VertexData);
		check(VertexData); // Initial requests should always yield a valid new vertex
	}
	AddVertices(MoveTemp(Vertices));
	LastActivityTime = FPlatformTime::Seconds();
}

FRequestCluster::FGraphSearch::~FGraphSearch()
{
	// Wait for any asynchronous tasks to complete by calling Poll and dropping the vertices it returns
	bool bIsDone = false;
	while (!bIsDone)
	{
		TArray<TUniquePtr<FVertexData>> VerticesToDrop;
		Poll(VerticesToDrop, bIsDone);
		if (!bIsDone)
		{
			UpdateDisplay();
			constexpr double WaitTime = 1.0;
			WaitForPollAvailability(WaitTime);
		}
	}
}

void FRequestCluster::FGraphSearch::AddVertices(TArray<TUniquePtr<FVertexData>>&& Vertices)
{
	TArray<FQueryVertexBatch*> NewBatches;
	{
		FScopeLock ScopeLock(&Lock);
		VerticesToRead.Reserve(VerticesToRead.Num() + Vertices.Num());
		for (TUniquePtr<FVertexData>& Vertex : Vertices)
		{
			VerticesToRead.Add(MoveTemp(Vertex));
		}
		NewBatches = CreateAvailableBatches();
	}
	for (FQueryVertexBatch* NewBatch : NewBatches)
	{
		NewBatch->Send();
	}
	Vertices.Reset();
}

void FRequestCluster::FGraphSearch::WaitForPollAvailability(double WaitTimeSeconds)
{
	uint32 WaitTime = (WaitTimeSeconds > 0.0) ? static_cast<uint32>(FMath::Floor(WaitTimeSeconds * 1000)) : MAX_uint32;
	PollReadyEvent->Wait(WaitTime);
}

void FRequestCluster::FGraphSearch::Poll(TArray<TUniquePtr<FVertexData>>& OutCompletedVertices, bool& bOutIsDone)
{
	bOutIsDone = true;
	TArray<FQueryVertexBatch*> NewBatches;
	{
		FScopeLock ScopeLock(&Lock);
		// We check for batches to send after adding dependencies for each vertex, so we can get them processing on the async thread
		// while we work on other vertices on the main thread.
		// We additionally need to check for them on every poll, in case the last batch we sent completes without finding any new vertices
		// and there are no batches remaining so we need to send an incomplete batch of the vertices remaining.
		NewBatches = CreateAvailableBatches();
		if (Batches.Num() != 0)
		{
			bOutIsDone = false;
			PollReadyEvent->Reset();
		}
	}
	for (FQueryVertexBatch* NewBatch : NewBatches)
	{
		NewBatch->Send();
	}

	OutCompletedVertices.Reset();
	for (;;)
	{
		TOptional<TUniquePtr<FVertexData>> VertexData = CompletedVertices.Dequeue();
		if (!VertexData.IsSet())
		{
			break;
		}
		OutCompletedVertices.Add(MoveTemp(*VertexData));
	}

	bOutIsDone = bOutIsDone && OutCompletedVertices.Num() == 0;
	if (OutCompletedVertices.Num() > 0)
	{
		LastActivityTime = FPlatformTime::Seconds();
	}
}

void FRequestCluster::FGraphSearch::UpdateDisplay()
{
	constexpr double WarningTimeout = 10.0;
	if (FPlatformTime::Seconds() > LastActivityTime + WarningTimeout && bCookAttachmentsEnabled)
	{
		FScopeLock ScopeLock(&Lock);
		int32 NumVertices = 0;
		int32 NumBatches = Batches.Num();
		for (TPair<FQueryVertexBatch*, TUniquePtr<FQueryVertexBatch>>& Pair : Batches)
		{
			NumVertices += Pair.Key->PendingVertices;
		}

		UE_LOG(LogCook, Warning, TEXT("FRequestCluster waited more than %.0lfs for previous build results from the oplog. ")
			TEXT("NumPendingBatches == %d, NumPendingVertices == %d. Continuing to wait..."),
			WarningTimeout, NumBatches, NumVertices);
		LastActivityTime = FPlatformTime::Seconds();
	}
}

void FRequestCluster::FGraphSearch::VisitVertex(const FVertexData& VertexData)
{
	// Only called from Process thread
	using namespace UE::AssetRegistry;
	using namespace UE::TargetDomain;

	TArray<FName>& HardDependencies(Scratch.HardDependencies);
	TArray<FName>& SoftDependencies(Scratch.SoftDependencies);
	HardDependencies.Reset();
	SoftDependencies.Reset();
	FPackageData* PackageData = VertexData.PackageData;
	if (VertexData.bExploreDependencies)
	{
		FName PackageName = PackageData->GetPackageName();

		// TODO EditorOnly References: We only fetch Game dependencies, because the cooker explicitly loads all of
		// the dependencies that we report. And if we explicitly load an EditorOnly dependency, that causes
		// StaticLoadObjectInternal to SetLoadedByEditorPropertiesOnly(false), which then treats the editor-only package
		// as needed in-game.
		EDependencyQuery DependencyQuery = EDependencyQuery::Game;

		Cluster.AssetRegistry.GetDependencies(PackageName, HardDependencies, EDependencyCategory::Package,
			DependencyQuery | EDependencyQuery::Hard);
		// We always skip assetregistry soft dependencies if the cook commandline is set to skip soft references.
		// We also need to skip them if the project has problems with editor-only robustness and has turned
		// ExploreDependencies off
		if (Cluster.bAllowSoftDependencies)
		{
			Cluster.AssetRegistry.GetDependencies(PackageName, SoftDependencies, EDependencyCategory::Package,
				DependencyQuery | EDependencyQuery::Soft);

			// Even if we're following soft references in general, we need to check with the SoftObjectPath registry
			// for any startup packages that marked their softobjectpaths as excluded, and not follow those
			TSet<FName>& SkippedPackages(Scratch.SkippedPackages);
			if (GRedirectCollector.RemoveAndCopySoftObjectPathExclusions(PackageName, SkippedPackages))
			{
				SoftDependencies.RemoveAll([&SkippedPackages](FName SoftDependency)
					{
						return SkippedPackages.Contains(SoftDependency);
					});
			}
		}

		bool bFoundCachedTargetDomain = false;
		bool bFoundBuildDefinitions = false;
		int32 NumPlatforms = GetNumPlatforms();
		// Platform 0 is the platform agnostic data, which we check after the loop
		for (int32 PlatformIndex = 1; PlatformIndex < NumPlatforms; ++PlatformIndex)
		{
			const FCookAttachments& PlatformAttachments = VertexData.CookAttachments[PlatformIndex];
			if (!IsCookAttachmentsValid(PackageName, PlatformAttachments))
			{
				continue;
			}
			const ITargetPlatform* TargetPlatform = Cluster.Platforms[PlatformIndex - 1];
			ICookedPackageWriter* PackageWriter = Cluster.PackageWriters[PlatformIndex - 1];
			if (!Cluster.bFullBuild && Cluster.bHybridIterativeEnabled)
			{
				if (IsIterativeEnabled(PackageName))
				{
					bFoundCachedTargetDomain = true;
					PackageData->SetPlatformCooked(TargetPlatform, true);
					PackageWriter->MarkPackagesUpToDate({ PackageName });
					// Declare the package to the EDLCookInfo verification so we don't warn about missing exports from it
					UE::SavePackageUtilities::EDLCookInfoAddIterativelySkippedPackage(PackageName);
				}
				HardDependencies.Append(PlatformAttachments.BuildDependencies);
				if (Cluster.bAllowSoftDependencies)
				{
					SoftDependencies.Append(PlatformAttachments.RuntimeOnlyDependencies);
				}

				if (Cluster.bPreQueueBuildDefinitions)
				{
					bFoundBuildDefinitions = true;
					Cluster.BuildDefinitions.AddBuildDefinitionList(PackageName, TargetPlatform,
						PlatformAttachments.BuildDefinitionList);
				}
			}
		}
		if (Cluster.bPreQueueBuildDefinitions && !bFoundBuildDefinitions)
		{
			const FCookAttachments& PlatformAgnosticAttachments = VertexData.CookAttachments[0];
			if (IsCookAttachmentsValid(PackageName, PlatformAgnosticAttachments))
			{
				Cluster.BuildDefinitions.AddBuildDefinitionList(PackageName, nullptr, PlatformAgnosticAttachments.BuildDefinitionList);
			}
		}
		if (bFoundCachedTargetDomain)
		{
			COOK_STAT(++DetailedCookStats::NumPackagesIterativelySkipped);
		}

		// Sort the lists of Dependencies to check for uniqueness and make them deterministic
		Algo::Sort(HardDependencies, FNameLexicalLess());
		HardDependencies.SetNum(Algo::Unique(HardDependencies));
		Algo::Sort(SoftDependencies, FNameLexicalLess());
		SoftDependencies.SetNum(Algo::Unique(SoftDependencies));
		SoftDependencies.RemoveAll([&HardDependencies](FName Dependency)
			{
				return Algo::BinarySearch(HardDependencies, Dependency, FNameLexicalLess()) != INDEX_NONE;
			});

		if (HardDependencies.Num() || SoftDependencies.Num())
		{
			TArray<FPackageData*>& Edges = GraphEdges.FindOrAdd(PackageData);
			check(Edges.Num() == 0);
			Edges.Reserve(HardDependencies.Num());
			TArray<TUniquePtr<FVertexData>>& NewVertices(Scratch.NewVertices);
			NewVertices.Reset(HardDependencies.Num() + SoftDependencies.Num());
			for (TArray<FName>* Dependencies : { &HardDependencies, &SoftDependencies })
			{
				bool bHardDependency = Dependencies == &HardDependencies;
				for (FName Dependency : *Dependencies)
				{
					// Process any CoreRedirects before checking whether the package exists
					FName Redirected = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package,
						FCoreRedirectObjectName(NAME_None, NAME_None, Dependency)).PackageName;
					Dependency = Redirected;

					TUniquePtr<FVertexData> NewVertexData;
					EInstigator InstigatorType = bHardDependency ? EInstigator::HardDependency : EInstigator::SoftDependency;
					FPackageData* DependencyPackageData = FindOrAddVertex(Dependency, nullptr,
						false /* bInitialRequest */, FInstigator(InstigatorType, PackageName), NewVertexData);
					if (NewVertexData)
					{
						NewVertices.Add(MoveTemp(NewVertexData));
					}
					if (bHardDependency && DependencyPackageData)
					{
						Edges.Add(DependencyPackageData);
					}
				}
			}
			if (NewVertices.Num())
			{
				AddVertices(MoveTemp(NewVertices));
			}
		}
	}

	for (const ITargetPlatform* TargetPlatform : Cluster.Platforms)
	{
		PackageData->FindOrAddPlatformData(TargetPlatform).bExplored = true;
	}
	bool bAlreadyCooked = PackageData->AreAllRequestedPlatformsCooked(true /* bAllowFailedCooks */);

	if (VertexData.bCookable && !bAlreadyCooked)
	{
		TransitiveRequests.Add(PackageData);
	}
	else
	{
		ESuppressCookReason SuppressCookReason = VertexData.bCookable ? ESuppressCookReason::AlreadyCooked : VertexData.SuppressCookReason;
		Cluster.RequestsToDemote.Emplace(PackageData, SuppressCookReason);
	}
}

FRequestCluster::FVertexData::FVertexData(EAsyncType, int32 NumPlatforms)
{
	CookAttachments.SetNum(NumPlatforms, true /* bAllowShrinking */);
	Reset();
}

FRequestCluster::FVertexData::FVertexData(ESkipDependenciesType, FPackageData& InPackageData, FRequestCluster& Cluster)
{
	PackageName = InPackageData.GetPackageName();
	PackageData = &InPackageData;
	bInitialRequest = true;
	bCookable = Cluster.IsRequestCookable(PackageData->GetPackageName(), PackageData, SuppressCookReason);
	bExploreDependencies = false;
}

void FRequestCluster::FVertexData::Reset()
{
	PackageName = NAME_None;
	for (UE::TargetDomain::FCookAttachments& PlatformAttachments : CookAttachments)
	{
		PlatformAttachments.Empty();
	}
	PackageData = nullptr;
	bInitialRequest = false;
	bCookable = false;
	bExploreDependencies = false;
	SuppressCookReason = ESuppressCookReason::InvalidSuppressCookReason;
}

TUniquePtr<FRequestCluster::FVertexData> FRequestCluster::FGraphSearch::AllocateVertex()
{
	// Only called from Process thread
	TUniquePtr<FVertexData> Result;
	Result = VertexAllocationPool.Num()
		? VertexAllocationPool.Pop(false /* bAllowShrinking */)
		: TUniquePtr<FVertexData>(new FVertexData(FVertexData::EAsync, GetNumPlatforms()));
	// Vertices are Reset when constructed or when returned to pool, so we do not need to reset here
	return Result;
}

void FRequestCluster::FGraphSearch::FreeVertex(TUniquePtr<FVertexData>&& Vertex)
{
	// Only called from Process thread
	Vertex->Reset();
	VertexAllocationPool.Add(MoveTemp(Vertex));
}

FPackageData* FRequestCluster::FGraphSearch::FindOrAddVertex(FName PackageName, FPackageData* PackageData,
	bool bInitialRequest, const FInstigator& InInstigator, TUniquePtr<FVertexData>& OutNewVertex)
{
	// Only called from Process thread
	OutNewVertex.Reset();
	FVisitStatus& VisitStatus = Visited.FindOrAdd(PackageName);
	if (VisitStatus.bVisited)
	{
		return VisitStatus.PackageData;
	}
	VisitStatus.bVisited = true;

	ESuppressCookReason SuppressCookReason;
	bool bCookable = Cluster.IsRequestCookable(PackageName, PackageData, SuppressCookReason);
	if (!bInitialRequest)
	{
		if (!bCookable)
		{
			return nullptr;
		}
		check(PackageData); // IsRequestCookable ensures PackageData if it returns true
		if (!Cluster.TryTakeOwnership(*PackageData, false /* bUrgent */, FCompletionCallback(), InInstigator))
		{
			return nullptr;
		}
		Cluster.OwnedPackageDatas.Add(PackageData);
	}

	VisitStatus.PackageData = PackageData;
	OutNewVertex = AllocateVertex();
	OutNewVertex->PackageName = PackageName;
	OutNewVertex->PackageData = PackageData;
	OutNewVertex->bInitialRequest = bInitialRequest;
	OutNewVertex->bCookable = bCookable;
	OutNewVertex->SuppressCookReason = SuppressCookReason;
	OutNewVertex->bExploreDependencies = bCookable;

	return PackageData;
}

TArray<FRequestCluster::FQueryVertexBatch*> FRequestCluster::FGraphSearch::CreateAvailableBatches()
{
	// Called from inside this->Lock
	TArray<FQueryVertexBatch*> Results;
	constexpr int32 BatchSize = 1000;
	Results.Reserve((VerticesToRead.Num() + BatchSize - 1) / BatchSize);
	while (VerticesToRead.Num() >= BatchSize)
	{
		Results.Add(CreateBatchOfPoppedVertices(BatchSize));
	}
	if (Batches.Num() == 0 && !bHasPendingVertices && VerticesToRead.Num() > 0)
	{
		// No more vertices are coming into VerticesToRead; send the batch that we have
		Results.Add(CreateBatchOfPoppedVertices(VerticesToRead.Num()));
	}
	return Results;
}

FRequestCluster::FHasPendingVerticesScope::FHasPendingVerticesScope(FGraphSearch& InGraphSearch)
	:GraphSearch(InGraphSearch)
{
	checkf(!GraphSearch.bHasPendingVertices, TEXT("Nested HasPendingVerticesScope is not yet supported"));
	GraphSearch.bHasPendingVertices = true;
}

FRequestCluster::FHasPendingVerticesScope::~FHasPendingVerticesScope()
{
	checkf(GraphSearch.bHasPendingVertices, TEXT("bHasPendingVertices was corrupted during FHasPendingVerticesScope"));
	GraphSearch.bHasPendingVertices = false;
	TArray<TUniquePtr<FVertexData>> Empty;
	// Call AddVertices to check whether a batch needs to be sent because no further vertices are pending
	GraphSearch.AddVertices(MoveTemp(Empty));
}

TUniquePtr<FRequestCluster::FQueryVertexBatch> FRequestCluster::FGraphSearch::AllocateBatch()
{
	// Called from inside this->Lock
	TUniquePtr<FQueryVertexBatch> Result;
	Result = BatchAllocationPool.Num()
		? BatchAllocationPool.Pop(false /* bAllowShrinking */)
		: TUniquePtr<FQueryVertexBatch>(new FQueryVertexBatch(*this));
	// Batches are Reset when constructed or when returned to pool, so we do not need to Reset here
	return Result;
}

void FRequestCluster::FGraphSearch::FreeBatch(TUniquePtr<FQueryVertexBatch>&& Batch)
{
	// Called from inside this->Lock
	Batch->Reset();
	BatchAllocationPool.Add(MoveTemp(Batch));
}

FRequestCluster::FQueryVertexBatch* FRequestCluster::FGraphSearch::CreateBatchOfPoppedVertices(int32 BatchSize)
{
	// Called from inside this->Lock
	check(BatchSize <= VerticesToRead.Num());
	TUniquePtr<FQueryVertexBatch> BatchData = AllocateBatch();
	BatchData->Vertices.Reserve(BatchSize);
	for (int32 BatchIndex = 0; BatchIndex < BatchSize; ++BatchIndex)
	{
		TUniquePtr<FVertexData> VertexData = VerticesToRead.PopFrontValue();
		TUniquePtr<FVertexData>& ExistingVert = BatchData->Vertices.FindOrAdd(VertexData->PackageName);
		check(!ExistingVert); // We should not have any duplicate names in VerticesToRead
		ExistingVert = MoveTemp(VertexData);
	}
	TUniquePtr<FQueryVertexBatch>& ExistingBatch = Batches.FindOrAdd(BatchData.Get());
	check(!ExistingBatch);
	ExistingBatch = MoveTemp(BatchData);
	return ExistingBatch.Get();
}

void FRequestCluster::FGraphSearch::OnBatchCompleted(FQueryVertexBatch* Batch)
{
	FScopeLock ScopeLock(&Lock);
	FreeBatch(Batches.FindAndRemoveChecked(Batch));
	PollReadyEvent->Trigger();
}

void FRequestCluster::FGraphSearch::OnVertexCompleted()
{
	// The trigger occurs outside of the lock, and might get clobbered and incorrectly ignored by a call to Poll which
	// consumes the vertices before our caller added a vertex but calls PollReadyEvent->Reset after this PollReadyEvent->Trigger.
	// This clobbering will not cause a deadlock, because eventually DestroyBatch will be called which triggers inside
	// the lock. Doing the per-vertex trigger outside the lock is good for performance.
	PollReadyEvent->Trigger();
}

FRequestCluster::FQueryVertexBatch::FQueryVertexBatch(FGraphSearch& InGraphSearch)
	: ThreadSafeOnlyVars(InGraphSearch)
	, NumPlatforms(InGraphSearch.GetNumPlatforms())
{
	Reset();
}

void FRequestCluster::FQueryVertexBatch::Reset()
{
	Scratch.PackageNames.Reset();
	Vertices.Reset();
}

void FRequestCluster::FQueryVertexBatch::Send()
{
	Scratch.PackageNames.Reserve(Vertices.Num());
	for (const TPair<FName, TUniquePtr<FVertexData>>& Pair : Vertices)
	{
		Scratch.PackageNames.Add(Pair.Key);
		Pair.Value->PendingPlatforms.store(NumPlatforms, std::memory_order_release);
	}
	PendingVertices.store(Vertices.Num(), std::memory_order_release);

	for (int32 PlatformIndex = 0; PlatformIndex < NumPlatforms; ++PlatformIndex)
	{
		const ITargetPlatform* TargetPlatform = nullptr;
		ICookedPackageWriter* PackageWriter = nullptr;
		// Platform 0 is the platform-agnostic platform
		if (PlatformIndex > 0)
		{
			TargetPlatform = ThreadSafeOnlyVars.Cluster.Platforms[PlatformIndex - 1];
			PackageWriter = ThreadSafeOnlyVars.Cluster.PackageWriters[PlatformIndex - 1];
		}

		TUniqueFunction<void(FName PackageName, UE::TargetDomain::FCookAttachments&& Result)> Callback =
			[this, PlatformIndex](FName PackageName, UE::TargetDomain::FCookAttachments&& Attachments)
		{
			RecordCacheResults(PackageName, PlatformIndex, MoveTemp(Attachments));
		};
		if (ThreadSafeOnlyVars.bCookAttachmentsEnabled)
		{
			UE::TargetDomain::FetchCookAttachments(Scratch.PackageNames, TargetPlatform,
				PackageWriter, MoveTemp(Callback));
		}
		else
		{
			// When we do not need to asynchronously fetch, we record empty cache results from an AsyncTask.
			// Using an AsyncTask keeps the threading flow similar to the FetchCookAttachments case
			AsyncTask(ENamedThreads::AnyThread,
				[PackageNames = Scratch.PackageNames, Callback = MoveTemp(Callback)]()
			{
				for (FName PackageName : PackageNames)
				{
					UE::TargetDomain::FCookAttachments Attachments;
					Callback(PackageName, MoveTemp(Attachments));
				}
			});
		}
	}
}

void FRequestCluster::FQueryVertexBatch::RecordCacheResults(FName PackageName, int32 PlatformIndex,
	UE::TargetDomain::FCookAttachments&& CookAttachments)
{
	TUniquePtr<FVertexData>& VertexData = Vertices.FindChecked(PackageName);
	check(VertexData);
	VertexData->CookAttachments[PlatformIndex] = MoveTemp(CookAttachments);
	if (VertexData->PendingPlatforms.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		ThreadSafeOnlyVars.CompletedVertices.Enqueue(MoveTemp(VertexData));
		bool bBatchComplete = PendingVertices.fetch_sub(1, std::memory_order_relaxed) == 1;
		if (!bBatchComplete)
		{
			ThreadSafeOnlyVars.OnVertexCompleted();
		}
		else
		{
			ThreadSafeOnlyVars.OnBatchCompleted(this);
			// this is no longer accessible
		}
	}
}

int32 FRequestCluster::FGraphSearch::GetNumPlatforms() const
{
	return Cluster.Platforms.Num() + 1;
}

TArray<FPackageData*>& FRequestCluster::FGraphSearch::GetTransitiveRequests()
{
	return TransitiveRequests;
}

TMap<FPackageData*, TArray<FPackageData*>>& FRequestCluster::FGraphSearch::GetGraphEdges()
{
	return GraphEdges;
}

bool FRequestCluster::IsRequestCookable(FName PackageName, FPackageData*& InOutPackageData,
	ESuppressCookReason& OutReason)
{
	return IsRequestCookable(PackageName, InOutPackageData, PackageDatas, PackageTracker,
		DLCPath, bErrorOnEngineContentUse, bAllowUncookedAssetReferences, GetPlatforms(), OutReason);
}

bool FRequestCluster::IsRequestCookable(FName PackageName, FPackageData*& InOutPackageData,
	FPackageDatas& InPackageDatas, FPackageTracker& InPackageTracker,
	FStringView InDLCPath, bool bInErrorOnEngineContentUse, bool bInAllowUncookedAssetReferences,
	TConstArrayView<const ITargetPlatform*> RequestPlatforms, ESuppressCookReason& OutReason)
{
	TStringBuilder<256> NameBuffer;
	// We need to reject packagenames from adding themselves or their transitive dependencies using all the same rules that
	// UCookOnTheFlyServer::ProcessRequest uses. Packages that are rejected from cook do not add their dependencies to the cook.
	PackageName.ToString(NameBuffer);
	if (FPackageName::IsScriptPackage(NameBuffer))
	{
		OutReason = ESuppressCookReason::ScriptPackage;
		return false;
	}

	if (!InOutPackageData)
	{
		InOutPackageData = InPackageDatas.TryAddPackageDataByPackageName(PackageName);
		if (!InOutPackageData)
		{
			// Package does not exist on disk
			OutReason = ESuppressCookReason::DoesNotExistInWorkspaceDomain;
			return false;
		}
	}

	FName FileName = InOutPackageData->GetFileName();
	if (InPackageTracker.NeverCookPackageList.Contains(FileName))
	{
		UE_LOG(LogCook, Verbose, TEXT("Package %s is referenced but is in the never cook package list, discarding request"), *NameBuffer);
		OutReason = ESuppressCookReason::NeverCook;
		return false;
	}


	if (bInErrorOnEngineContentUse && !InDLCPath.IsEmpty())
	{
		FileName.ToString(NameBuffer);
		if (!FStringView(NameBuffer).StartsWith(InDLCPath))
		{
			if (!InOutPackageData->HasAllCookedPlatforms(RequestPlatforms, true /* bIncludeFailed */))
			{
				// AllowUncookedAssetReferences should only be used when the DLC plugin to cook is going to be mounted where uncooked packages are available.
				// This will allow a DLC plugin to be recooked continually and mounted in an uncooked editor which is useful for CI.
				if (!bInAllowUncookedAssetReferences)
				{
					UE_LOG(LogCook, Error, TEXT("Uncooked Engine or Game content %s is being referenced by DLC!"), *NameBuffer);
				}
			}
			OutReason = ESuppressCookReason::NotInCurrentPlugin;
			return false;
		}
	}

	OutReason = ESuppressCookReason::InvalidSuppressCookReason;
	return true;
}

} // namespace UE::Cook
