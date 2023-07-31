// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Set.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "Engine/ICookInfo.h"
#include "HAL/Event.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/MpscQueue.h"
#include "Containers/RingBuffer.h"
#include "Cooker/CookTypes.h"
#include "HAL/CriticalSection.h"
#include "TargetDomain/TargetDomainUtils.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

#include <atomic>

class IAssetRegistry;
class ICookedPackageWriter;
class ITargetPlatform;
class UCookOnTheFlyServer;
namespace UE::Cook { class FRequestQueue; }
namespace UE::Cook { struct FFilePlatformRequest; }
namespace UE::Cook { struct FPackageDatas; }
namespace UE::Cook { struct FPackageTracker; }

namespace UE::Cook
{
struct FPackageData;

/**
 * A group of external requests sent to CookOnTheFlyServer's tick loop. Transitive dependencies are found and all of the
 * requested or dependent packagenames are added as requests together to the cooking state machine.
 */
class FRequestCluster
{
public:
	/** The cluster's data about a package from the external requests or discovered dependency. */
	struct FFileNameRequest
	{
		FName FileName;
		FInstigator Instigator;
		FCompletionCallback CompletionCallback;
		bool bUrgent;

		FFileNameRequest(FFilePlatformRequest&& FileRequest, bool bInUrgent);
	};
	explicit FRequestCluster(UCookOnTheFlyServer& COTFS, TArray<const ITargetPlatform*>&& Platforms);
	explicit FRequestCluster(UCookOnTheFlyServer& COTFS, TConstArrayView<const ITargetPlatform*> Platforms);
	FRequestCluster(FRequestCluster&&) = default;

	/**
	 * Calculate the information needed to create a PackageData, and transitive search dependencies for all requests.
	 * Called repeatedly (due to timeslicing) until bOutComplete is set to true.
	 */
	void Process(const FCookerTimer& CookerTimer, bool& bOutComplete);

	TConstArrayView<const ITargetPlatform*> GetPlatforms() const { return Platforms; }
	/** PackageData container interface: return the number of PackageDatas owned by this container. */
	int32 NumPackageDatas() const;
	/** PackageData container interface: remove the PackageData from this container. */
	void RemovePackageData(FPackageData* PackageData);
	/** PackageData container interface: whether the PackageData is owned by this container. */
	bool Contains(FPackageData* PackageData) const;
	/**
	 * Remove all PackageDatas owned by this container and return them.
	 * OutRequestsToLoad is the set of PackageDatas sorted by leaf to root load order.
	 * OutRequestToDemote is the set of Packages that are uncookable or have already been cooked.
	 * If called before Process sets bOutComplete=true, all packages are put in OutRequestToLoad and are unsorted.
	 */
	void ClearAndDetachOwnedPackageDatas(TArray<FPackageData*>& OutRequestsToLoad,
		TArray<TPair<FPackageData*, ESuppressCookReason>>& OutRequestsToDemote,
		TMap<FPackageData*, TArray<FPackageData*>>& OutRequestGraph);

	/**
	 * Create clusters(s) for all the given name or packagedata requests and append them to OutClusters.
	 * Multiple clusters are necessary if the list of platforms differs between requests.
	 */
	static void AddClusters(UCookOnTheFlyServer& COTFS, TArray<FFilePlatformRequest>&& InRequests,
		bool bRequestsAreUrgent, TRingBuffer<FRequestCluster>& OutClusters);
	static void AddClusters(UCookOnTheFlyServer& COTFS, FPackageDataSet& UnclusteredRequests,
		TRingBuffer<FRequestCluster>& OutClusters, FRequestQueue& QueueForReadyRequests);

private:
	struct FGraphSearch;

	/** GraphSearch cached data for a packagename that has already been visited. */
	struct FVisitStatus
	{
		FPackageData* PackageData = nullptr;
		bool bVisited = false;
	};

	/**
	 * GraphSearch data for an in-progress package. VertexData is created when a package is discovered
	 * from the dependencies of a referencer package. It is in-progress while its own dependencies
	 * are looked up asynchronously. The VertexData is freed after those dependencies have been logged
	 * and cook flags have been set on the FPackageData for the package.
	 */
	struct FVertexData
	{
		enum ESkipDependenciesType
		{
			ESkipDependencies
		};
		enum EAsyncType
		{
			EAsync
		};
		FVertexData(EAsyncType, int32 NumPlatforms);
		FVertexData(ESkipDependenciesType, FPackageData& PackageData, FRequestCluster& Cluster);
		void Reset();

		/** Data looked up about the package's dependencies from the PackageWriter's previous cook of the package */
		TArray<UE::TargetDomain::FCookAttachments> CookAttachments;
		/** Copy of PackageData->GetPackageName() */
		FName PackageName;
		/**
		 * Not dereferenceable since it may be removed while in use on async threads.
		 * Used as an identifier when the vertex is returned to the process thread.
		 */
		UE::Cook::FPackageData* PackageData;
		/** Number of asynchronous results from PackageWriter still being waited on. */
		std::atomic<uint32> PendingPlatforms;
		/**
		 * Written/Read when VertexData is allocated for a package name.
		 * It is Set to false if the packagename should not be part of the graph search.
		 */
		bool bValid;
		/** True if the package is from the Cluster's initial requests. Initial requests are always visited. */
		bool bInitialRequest;
		/** True if the package is cookable. Non-cookable packages are demoted back to idle. */
		bool bCookable;
		/** True if dependencies should be explored, based on other properties of the VertexData. */
		bool bExploreDependencies;
		/** The reason it is not cookable if !bCookable, or ESuppressCookReason::InvalidSuppressCookReason if bCookable. */
		ESuppressCookReason SuppressCookReason;
	};

	/**
	 * Each FVertexData includes has-been-cooked existence and dependency information that is looked up
	 * from PackageWriter storage of previous cooks. The lookup can have significant latency and per-query
	 * costs. We therefore do the lookups for vertices asynchronously and in batches. An FQueryVertexBatch
	 * is a collection of FVertexData that are sent in a single lookup batch. The batch is destroyed
	 * once the results for all requested vertices are received.
	 */
	struct FQueryVertexBatch
	{
		FQueryVertexBatch(FGraphSearch& InGraphSearch);
		void Reset();
		void Send();

		void RecordCacheResults(FName PackageName, int32 PlatformIndex,
			UE::TargetDomain::FCookAttachments&& CookAttachments);

		struct FScratch
		{
			TArray<FName> PackageNames;
		};

		/** Scratch-space variables that are reused to avoid allocations. */
		FScratch Scratch;
		/**
		 * Map of the requested vertices by name. The keys in the map are created during Send and are
		 * read-only afterwards (so the map is multithread-readable). The value for a given 
		 * key is deleted when the results for the vertex are received and the FVertexData is
		 * moved to CompletedVertices on the FGraphSearch.
		 * */
		TMap<FName, TUniquePtr<FVertexData>> Vertices;
		/** Accessor for the GraphSearch; only thread-safe functions and variables should be accessed. */
		FGraphSearch& ThreadSafeOnlyVars;
		/** Copy of NumPlatforms from GraphSearch; read-only */
		int32 NumPlatforms;
		/** The number of vertices that are still awaiting results. This batch is closed when PendingVertices == 0. */
		std::atomic<uint32> PendingVertices;
	};

	/** A scope to notify GraphSearch that more vertices are coming and it should wait for a full batch to send. */
	struct FHasPendingVerticesScope
	{
		FHasPendingVerticesScope(FGraphSearch& InGraphSearch);
		~FHasPendingVerticesScope();

		FGraphSearch& GraphSearch;
	};

	/**
	 * Variables and functions that are only used during FetchDependencies. FetchDependencies executes a graph search
	 * over the graph of packages (vertices) and their hard/soft dependencies upon other packages (edges). 
	 * Finding the dependencies for each package uses previous cook results and is executed asynchronously.
	 * After the graph is searched, packages are sorted topologically from leaf to root, so that packages are
	 * loaded/saved by the cook before the packages that need them to be in memory to load.
	 */
	struct FGraphSearch
	{
	public:
		/** Data used during the reentrant dependencies and topological sort operation of FRequestCluster. */
		FGraphSearch(FRequestCluster& InCluster);
		~FGraphSearch();

		// All public functions are callable only from the process thread
		/**
		 * Called from the Process Thread to consume FVertexData that have finished their async work and are
		 * ready for the processing that must happen on the process thread
		 */
		void Poll(TArray<TUniquePtr<FVertexData>>& OutCompletedVertices, bool& bOutIsDone);
		/** Sleep (with timeout) until results are ready in Poll */
		void WaitForPollAvailability(double WaitTimeSeconds);
		/** Log diagnostic information about the search, e.g. timeout warnings. */
		void UpdateDisplay();
		/** AddVertices that need async processing */
		void AddVertices(TArray<TUniquePtr<FVertexData>>&& Vertices);
		/**
		 * Record a vertex's cook flags and dependencies on the PackageData.
		 * Create and send any new vertex dependencies for async work. Called from processs-thread only.
		 */
		void VisitVertex(const FVertexData& VertexData);
		/**
		 * Find the PackageData for the given PackageName if it has been visited before.
		 * If not, construct a new vertex to be async processed and visited.
		 */
		FPackageData* FindOrAddVertex(FName PackageName, FPackageData* PackageData, bool bInitialRequest,
			const FInstigator& InInstigator, TUniquePtr<FVertexData>& OutNewVertex);
		/** Allocate memory for a new vertex; returned vertex is not yet constructed. */
		TUniquePtr<FVertexData> AllocateVertex();
		/** Free an allocated vertex. */ 
		void FreeVertex(TUniquePtr<FVertexData>&& Batch);
		/** PackageDatas found during the graph search, including the initial requests. */
		TArray<FPackageData*>& GetTransitiveRequests();
		/**
		 * Edges in the dependency graph found during graph search.
		 * Only includes PackageDatas that are part of this cluster
		 */
		TMap<FPackageData*, TArray<FPackageData*>>& GetGraphEdges();

	private:
		struct FScratch
		{
			TArray<FName> HardDependencies;
			TArray<FName> SoftDependencies;
			TArray<TUniquePtr<FVertexData>> NewVertices;
			TSet<FName> SkippedPackages;
		};
		friend struct FQueryVertexBatch;
		friend struct FHasPendingVerticesScope;
		
		// Functions that must be called only within the Lock
		/** Allocate memory for a new batch; returned batch is not yet constructed. */
		TUniquePtr<FQueryVertexBatch> AllocateBatch();
		/** Free an allocated batch. */ 
		void FreeBatch(TUniquePtr<FQueryVertexBatch>&& Batch);
		/** Pop vertices from VerticesToRead into batches, if there are enough of them. */
		TArray<FQueryVertexBatch*> CreateAvailableBatches();
		/** Pop a single batch vertices from VerticesToRead. */
		FQueryVertexBatch* CreateBatchOfPoppedVertices(int32 BatchSize);

		// Functions that are safe to call from any thread
		/** Notify process thread of batch completion and deallocate it. */
		void OnBatchCompleted(FQueryVertexBatch* Batch);
		/** Notify process thread of vertex completion. */
		void OnVertexCompleted();
		/**
		 * Get the number of platforms sent to FetchCookAttachments.
		 * This includes +1 for null platform that fetches platform-agnostic data.
		 **/
		int32 GetNumPlatforms() const;

	private:
		// Variables that are read-only during multithreading
		FRequestCluster& Cluster;
		bool bCookAttachmentsEnabled = false;

		// Variables that are accessible only from the Process thread
		/** Scratch-space variables that are reused to avoid allocations. */
		FScratch Scratch;
		TArray<FPackageData*> TransitiveRequests;
		TMap<FPackageData*, TArray<FPackageData*>> GraphEdges;
		/** Which PackageNames have already been inspected, and either discarded or a Vertex created for them. */
		TMap<FName, FVisitStatus> Visited;
		TArray<TUniquePtr<FVertexData>> VertexAllocationPool;
		TRingBuffer<TUniquePtr<FVertexData>> VerticesToRead;
		/** Time-tracker for timeout warnings in Poll */
		double LastActivityTime;
		/** A stack-scope variable to control automatic CreateAvailableBatches behavior. */
		bool bHasPendingVertices = false;

		// Variables that are accessible from multiple threads, guarded by Lock
		FCriticalSection Lock;
		TArray<TUniquePtr<FQueryVertexBatch>> BatchAllocationPool;
		TMap<FQueryVertexBatch*, TUniquePtr<FQueryVertexBatch>> Batches;

		// Variables that are accessible from multiple threads, internally threadsafe
		TMpscQueue<TUniquePtr<FVertexData>> CompletedVertices;
		FEventRef PollReadyEvent;
	};

	void Initialize(UCookOnTheFlyServer& COTFS);
	void FetchPackageNames(const FCookerTimer& CookerTimer, bool& bOutComplete);
	void FetchDependencies(const FCookerTimer& CookerTimer, bool& bOutComplete);
	void StartAsync(const FCookerTimer& CookerTimer, bool& bOutComplete);
	bool TryTakeOwnership(FPackageData& PackageData, bool bUrgent, FCompletionCallback&& CompletionCallback,
		const FInstigator& InInstigator);
	bool IsRequestCookable(FName PackageName, FPackageData*& InOutPackageData, ESuppressCookReason& OutReason);
	static bool IsRequestCookable(FName PackageName, FPackageData*& InOutPackageData,
		FPackageDatas& InPackageDatas, FPackageTracker& InPackageTracker,
		FStringView InDLCPath, bool bInErrorOnEngineContentUse, bool bInAllowUncookedAssetReferences,
		TConstArrayView<const ITargetPlatform*> RequestPlatforms, ESuppressCookReason& OutReason);

	TArray<FFileNameRequest> InRequests;
	TArray<FPackageData*> Requests;
	TArray<TPair<FPackageData*, ESuppressCookReason>> RequestsToDemote;
	TArray<const ITargetPlatform*> Platforms;
	TArray<ICookedPackageWriter*> PackageWriters;
	FPackageDataSet OwnedPackageDatas;
	TMap<FPackageData*, TArray<FPackageData*>> RequestGraph;
	FString DLCPath;
	TUniquePtr<FGraphSearch> GraphSearch; // Needs to be dynamic-allocated because of large alignment
	UCookOnTheFlyServer& COTFS;
	FPackageDatas& PackageDatas;
	IAssetRegistry& AssetRegistry;
	FPackageTracker& PackageTracker;
	FBuildDefinitions& BuildDefinitions;
	int32 NextRequest = 0;
	bool bAllowHardDependencies = true;
	bool bAllowSoftDependencies = true;
	bool bHybridIterativeEnabled = true;
	bool bErrorOnEngineContentUse = false;
	bool bAllowUncookedAssetReferences = false;
	bool bPackageNamesComplete = false;
	bool bDependenciesComplete = false;
	bool bStartAsyncComplete = false;
	bool bFullBuild = false;
	bool bPreQueueBuildDefinitions = true;
};

}
