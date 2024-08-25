// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/BitArray.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Set.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "HAL/Event.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/MpscQueue.h"
#include "Containers/RingBuffer.h"
#include "Cooker/CookTypes.h"
#include "Cooker/TypedBlockAllocator.h"
#include "HAL/CriticalSection.h"
#include "TargetDomain/TargetDomainUtils.h"
#include "Templates/UniquePtr.h"
#include "UObject/ICookInfo.h"
#include "UObject/NameTypes.h"

#include <atomic>

class IAssetRegistry;
class ICookedPackageWriter;
class ITargetPlatform;
class UCookOnTheFlyServer;
namespace UE::Cook { class FRequestQueue; }
namespace UE::Cook { struct FDiscoveryQueueElement; }
namespace UE::Cook { struct FFilePlatformRequest; }
namespace UE::Cook { struct FPackageData; }
namespace UE::Cook { struct FPackageDatas; }
namespace UE::Cook { struct FPackagePlatformData; }
namespace UE::Cook { struct FPackageTracker; }

namespace UE::Cook
{

/**
 * A group of external requests sent to CookOnTheFlyServer's tick loop. Transitive dependencies are found and all of the
 * requested or dependent packagenames are added as requests together to the cooking state machine.
 */
class FRequestCluster
{
public:
	FRequestCluster(UCookOnTheFlyServer& COTFS, TArray<FFilePlatformRequest>&& InRequests);
	FRequestCluster(UCookOnTheFlyServer& COTFS, FPackageDataSet&& InRequests);
	FRequestCluster(UCookOnTheFlyServer& COTFS, TRingBuffer<FDiscoveryQueueElement>& DiscoveryQueue);
	FRequestCluster(FRequestCluster&&) = default;

	/**
	 * Calculate the information needed to create a PackageData, and transitive search dependencies for all requests.
	 * Called repeatedly (due to timeslicing) until bOutComplete is set to true.
	 */
	void Process(const FCookerTimer& CookerTimer, bool& bOutComplete);

	/** PackageData container interface: return the number of PackageDatas owned by this container. */
	int32 NumPackageDatas() const;
	/** PackageData container interface: remove the PackageData from this container. */
	void RemovePackageData(FPackageData* PackageData);
	void OnNewReachablePlatforms(FPackageData* PackageData);
	void OnPlatformAddedToSession(const ITargetPlatform* TargetPlatform);
	void OnRemoveSessionPlatform(const ITargetPlatform* TargetPlatform);
	void RemapTargetPlatforms(TMap<ITargetPlatform*, ITargetPlatform*>& Remap);

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

	static TConstArrayView<FName> GetLocalizationReferences(FName PackageName, UCookOnTheFlyServer& InCOTFS);
	static TArray<FName> GetAssetManagerReferences(FName PackageName);
	static void IsRequestCookable(const ITargetPlatform* TargetPlatform, FPackageData& PackageData,
		UCookOnTheFlyServer& COTFS, ESuppressCookReason& OutReason, bool& bOutCookable, bool& bOutExplorable);

private:
	struct FGraphSearch;

	/** GraphSearch cached data for a packagename that has already been visited. */
	struct FVisitStatus
	{
		FPackageData* PackageData = nullptr;
		bool bVisited = false;
	};

	/** Per-platform data in an active query for a vertex's dependencies/previous incremental results. */
	struct FQueryPlatformData
	{
		/** Data looked up about the package's dependencies from the PackageWriter's previous cook of the package */
		UE::TargetDomain::FCookAttachments CookAttachments;
		/**
		 * Platforms present in the FVertexQueryData are recorded as a an array of all possible platforms, with
		 * bActive=true for the ones that are present.
		 */
		bool bActive = false;
	};

	/** Input/output data for an active query for a vertex's dependencies/previous incrementa results. */
	struct FVertexQueryData
	{
		void Reset();

		FName PackageName;
		/** Settings and Results for each of the GraphSearch's FetchPlatforms. Element n corresponds to FetchPlatform n. */
		TArray<FQueryPlatformData> Platforms;
		/** Number of asynchronous results from PackageWriter still being waited on. */
		std::atomic<uint32> PendingPlatforms;
	};

	/**
	 * GraphSearch data for an package referenced by the cluster. VertexData is created when a package is discovered
	 * from the dependencies of a referencer package. It remains allocated for the rest of the Cluster's lifetime.
	 */
	struct FVertexData
	{
		UE::Cook::FPackageData* PackageData = nullptr;
		/** Non-null if there as a query active for the vertex's dependencies/previous incremental results. */
		FVertexQueryData* QueryData = nullptr;
		bool bAnyCookable = true;
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

		struct FPlatformData
		{
			TArray<FName> PackageNames;
		};

		TArray<FPlatformData> PlatformDatas;
		/**
		 * Map of the requested vertices by name. The map is created during Send and is
		 * read-only afterwards (so the map is multithread-readable). The Vertices pointed to have their own
		 * rules for what is accessible from the async work threads.
		 * */
		TMap<FName, FVertexData*> Vertices;
		/** Accessor for the GraphSearch; only thread-safe functions and variables should be accessed. */
		FGraphSearch& ThreadSafeOnlyVars;
		/** The number of vertices that are still awaiting results. This batch is closed when PendingVertices == 0. */
		std::atomic<uint32> PendingVertices;
	};

	/** Platform information that is constant (usually, some events can change it) during the cluster's lifetime. */
	struct FFetchPlatformData
	{
		const ITargetPlatform* Platform = nullptr;
		ICookedPackageWriter* Writer = nullptr;
		bool bIsPlatformAgnosticPlatform = false;
		bool bIsCookerLoadingPlatform = false;
	};
	// Platforms are listed in various arrays, always in the same order. Some special case entries exist and are added
	// at specified indices in the arrays.
	static constexpr int32 PlatformAgnosticPlatformIndex = 0;
	static constexpr int32 CookerLoadingPlatformIndex = 1;
	static constexpr int32 FirstSessionPlatformIndex = 2;

	/** How much traversal the GraphSearch should do based on settings for the entire cook. */
	enum class ETraversalTier
	{
		/** Do not fetch any edgedata. Used on CookWorkers; the director already did the fetch. */
		None,
		/**
		 * Fetch the edgedata and use it for ancillary calculation like updating whether a package is iteratively
		 * unmodified. Do not explore the discovered dependencies.
		 */
		FetchEdgeData,
		/** Fetch the edgedata, update ancillary calculations, and explore the discovered dependencies. */
		FollowDependencies,
		All=FollowDependencies,
	};

	/**
	 * Variables and functions that are only used during PumpExploration. PumpExploration executes a graph search
	 * over the graph of packages (vertices) and their hard/soft dependencies upon other packages (edges). 
	 * Finding the dependencies for each package uses previous cook results and is executed asynchronously.
	 * After the graph is searched, packages are sorted topologically from leaf to root, so that packages are
	 * loaded/saved by the cook before the packages that need them to be in memory to load.
	 */
	struct FGraphSearch
	{
	public:
		FGraphSearch(FRequestCluster& InCluster, ETraversalTier TraversalTier);
		~FGraphSearch();

		// All public functions are callable only from the process thread
		/** Skip the entire GraphSearch and just visit the Cluster's current OwnedPackageDatas. */
		void VisitWithoutDependencies();
		/** Start a search from the Cluster's current OwnedPackageDatas. */
		void StartSearch();
		void RemovePackageData(FPackageData* PackageData);
		void OnNewReachablePlatforms(FPackageData* PackageData);

		/**
		 * Visit newly reachable PackageDatas, queue a fetch of their dependencies, harvest new reachable PackageDatas
		 * from the results of the fetch.
		 */
		void TickExploration(bool& bOutDone);
		/** Sleep (with timeout) until work is available in TickExploration */
		void WaitForAsyncQueue(double WaitTimeSeconds);

		/**
		 * Edges in the dependency graph found during graph search.
		 * Only includes PackageDatas that are part of this cluster
		 */
		TMap<FPackageData*, TArray<FPackageData*>>& GetGraphEdges();

	private:
		// Scratch data structures used to avoid dynamic allocations; lifetime of each use is only on the stack
		struct FScratchPlatformDependencyBits
		{
			TBitArray<> HasPlatformByIndex;
			EInstigator InstigatorType = EInstigator::SoftDependency;
		};
		struct FScratch
		{
			TArray<FName> HardGameDependencies;
			TArray<FName> HardEditorDependencies;
			TArray<FName> SoftGameDependencies;
			TArray<FName> CookerLoadingDependencies;
			TMap<FName, FScratchPlatformDependencyBits> PlatformDependencyMap;
			TSet<FName> HardDependenciesSet;
			TSet<FName> SkippedPackages;
		};
		friend struct FQueryVertexBatch;
		friend struct FVertexData;
		
		// Functions callable only from the Process thread
		/** Log diagnostic information about the search, e.g. timeout warnings. */
		void UpdateDisplay();

		/** Asynchronously fetch the dependencies and previous incremental results for a vertex */
		void QueueEdgesFetch(FVertexData& Vertex, TConstArrayView<const ITargetPlatform*> Platforms);
		/** Calculate and store the vertex's PackageData's cookability for each reachable platform. Kick off edges fetch. */
		void VisitVertex(FVertexData& VertexData);
		/** Calculate and store the vertex's PackageData's cookability for the platform. */
		void VisitVertexForPlatform(FVertexData& VertexData, const ITargetPlatform* Platform,
			FPackagePlatformData& PlatformData, ESuppressCookReason& AccumulatedSuppressCookReason);
		 /** Process the results from async edges fetch and queue the found dependencies-for-visiting. */
		void ExploreVertexEdges(FVertexData& VertexData);

		/** Find or add a Vertex for PackageName. If PackageData is provided, use it, otherwise look it up. */
		FVertexData& FindOrAddVertex(FName PackageName);
		FVertexData& FindOrAddVertex(FName PackageName, FPackageData& PackageData);
		/** Batched allocation for vertices. */
		FVertexData* AllocateVertex();
		/** Batched allocation and free for QueryData. */
		FVertexQueryData* AllocateQueryData();
		void FreeQueryData(FVertexQueryData* QueryData);
		/** Queue a vertex for visiting and dependency traversal */
		void AddToFrontier(FVertexData& Vertex);

		// Functions that must be called only within the Lock
		/** Allocate memory for a new batch; returned batch is not yet constructed. */
		FQueryVertexBatch* AllocateBatch();
		/** Free an allocated batch. */ 
		void FreeBatch(FQueryVertexBatch* Batch);
		/** Pop vertices from VerticesToRead into batches, if there are enough of them. */
		void CreateAvailableBatches(bool bAllowIncompleteBatch);
		/** Pop a single batch vertices from VerticesToRead. */
		FQueryVertexBatch* CreateBatchOfPoppedVertices(int32 BatchSize);

		// Functions that are safe to call from any thread
		/** Notify process thread of batch completion and deallocate it. */
		void OnBatchCompleted(FQueryVertexBatch* Batch);
		/** Notify process thread of vertex completion. */
		void OnVertexCompleted();

		/** Total number of platforms known to the cluster, including the special cases. */
		int32 NumFetchPlatforms() const { return FetchPlatforms.Num(); }
		/** Total number of non-special-case platforms known to the cluster.Identical to COTFS's session platforms */
		int32 NumSessionPlatforms() const { return FetchPlatforms.Num() - 2; }

	private:
		// Variables that are read-only during multithreading
		TArray<FFetchPlatformData> FetchPlatforms;
		FRequestCluster& Cluster;
		ETraversalTier TraversalTier = ETraversalTier::All;

		// Variables that are accessible only from the Process thread
		/** Scratch-space variables that are reused to avoid allocations. */
		FScratch Scratch;
		TMap<FPackageData*, TArray<FPackageData*>> GraphEdges;
		TMap<FName, FVertexData*> Vertices;
		TSet<FVertexData*> Frontier;
		TTypedBlockAllocatorFreeList<FVertexData> VertexAllocator;
		TTypedBlockAllocatorResetList<FVertexQueryData> VertexQueryAllocator;
		/** Vertices queued for async processing that are not yet numerous enough to fill a batch. */
		TRingBuffer<FVertexData*> PreAsyncQueue;
		/** Time-tracker for timeout warnings in Poll */
		double LastActivityTime;

		// Variables that are accessible from multiple threads, guarded by Lock
		FCriticalSection Lock;
		TTypedBlockAllocatorResetList<FQueryVertexBatch> BatchAllocator;
		TSet<FQueryVertexBatch*> AsyncQueueBatches;

		// Variables that are accessible from multiple threads, internally threadsafe
		TMpscQueue<FVertexData*> AsyncQueueResults;
		FEventRef AsyncResultsReadyEvent;
	};

private:
	explicit FRequestCluster(UCookOnTheFlyServer& COTFS);
	void ReserveInitialRequests(int32 RequestNum);
	void PullIntoCluster(FPackageData& PackageData);
	void FetchPackageNames(const FCookerTimer& CookerTimer, bool& bOutComplete);
	void PumpExploration(const FCookerTimer& CookerTimer, bool& bOutComplete);
	void StartAsync(const FCookerTimer& CookerTimer, bool& bOutComplete);
	bool IsIncrementalCook() const;
	void IsRequestCookable(const ITargetPlatform* TargetPlatform, FName PackageName, FPackageData& PackageData,
		ESuppressCookReason& OutReason, bool& bOutCookable, bool& bOutExplorable);
	static void IsRequestCookable(const ITargetPlatform* TargetPlatform, FName PackageName, FPackageData& PackageData,
		UCookOnTheFlyServer& InCOTFS, FStringView InDLCPath, ESuppressCookReason& OutReason, bool& bOutCookable,
		bool& bOutExplorable);
	static void RandomizeCookOrder(TArray<FPackageData*>& InOutLeafToRootOrder,
		const TMap<FPackageData*, TArray<FPackageData*>>& Edges);

	TArray<FFilePlatformRequest> FilePlatformRequests;
	TFastPointerMap<FPackageData*, ESuppressCookReason> OwnedPackageDatas;
	TMap<FPackageData*, TArray<FPackageData*>> RequestGraph;
	FString DLCPath;
	TUniquePtr<FGraphSearch> GraphSearch; // Needs to be dynamic-allocated because of large alignment
	UCookOnTheFlyServer& COTFS;
	FPackageDatas& PackageDatas;
	IAssetRegistry& AssetRegistry;
	FPackageTracker& PackageTracker;
	FBuildDefinitions& BuildDefinitions;
	bool bAllowHardDependencies = true;
	bool bAllowSoftDependencies = true;
	bool bErrorOnEngineContentUse = false;
	bool bPackageNamesComplete = false;
	bool bDependenciesComplete = false;
	bool bStartAsyncComplete = false;
	bool bAllowIterativeResults = false;
	bool bPreQueueBuildDefinitions = true;
};

}
