// Copyright Epic Games, Inc. All Rights Reserved.

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Compare.h"
#include "Algo/Find.h"
#include "Algo/MinElement.h"
#include "Containers/Array.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheStore.h"
#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataLegacyCacheStore.h"
#include "DerivedDataRequest.h"
#include "DerivedDataRequestOwner.h"
#include "HAL/CriticalSection.h"
#include "MemoryCacheStore.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/ScopeRWLock.h"
#include "Serialization/CompactBinary.h"
#include "Templates/Invoke.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include <atomic>

namespace UE::DerivedData
{

ILegacyCacheStore* CreateCacheStoreAsync(ILegacyCacheStore* InnerCache, ECacheStoreFlags InnerFlags, IMemoryCacheStore* MemoryCache);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FCacheStoreHierarchy final : public ILegacyCacheStore, public ICacheStoreOwner
{
public:
	explicit FCacheStoreHierarchy(IMemoryCacheStore* MemoryCache);
	~FCacheStoreHierarchy() final = default;

	void Add(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags) final;

	void SetFlags(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags) final;

	void RemoveNotSafe(ILegacyCacheStore* CacheStore) final;

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) final;

	void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final;

	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) final;

	void GetValue(
		TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete) final;

	void GetChunks(
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) final;

	void LegacyStats(FDerivedDataCacheStatsNode& OutNode) final;

	bool LegacyDebugOptions(FBackendDebugOptions& Options) final;

	template <typename PutRequestType, typename GetRequestType> struct TBatchParams;
	using FCacheRecordBatchParams = TBatchParams<FCachePutRequest, FCacheGetRequest>;
	using FCacheValueBatchParams = TBatchParams<FCachePutValueRequest, FCacheGetValueRequest>;

private:
	// Caller must hold a write lock on CacheStoresLock.
	void UpdateNodeFlags();

	class FCounterEvent;

	class FBatchBase;
	template <typename Params> class TPutBatch;
	template <typename Params> class TGetBatch;

	class FGetChunksBatch;

	enum class ECacheStoreNodeFlags : uint32;
	FRIEND_ENUM_CLASS_FLAGS(ECacheStoreNodeFlags);

	static ECachePolicy GetCombinedPolicy(const ECachePolicy Policy) { return Policy; }
	static ECachePolicy GetCombinedPolicy(const FCacheRecordPolicy& Policy) { return Policy.GetRecordPolicy(); }

	static ECachePolicy AddPolicy(ECachePolicy BasePolicy, ECachePolicy Policy) { return BasePolicy | (Policy & ~FCacheValuePolicy::PolicyMask); }
	static ECachePolicy RemovePolicy(ECachePolicy BasePolicy, ECachePolicy Policy) { return BasePolicy & ~Policy; }
	static FCacheRecordPolicy AddPolicy(const FCacheRecordPolicy& BasePolicy, ECachePolicy Policy);
	static FCacheRecordPolicy RemovePolicy(const FCacheRecordPolicy& BasePolicy, ECachePolicy Policy);

	static bool CanQuery(ECachePolicy Policy, ECacheStoreFlags Flags);
	static bool CanStore(ECachePolicy Policy, ECacheStoreFlags Flags);
	static bool CanStoreIfOk(ECachePolicy Policy, ECacheStoreNodeFlags Flags);
	static bool CanQueryIfError(ECachePolicy Policy, ECacheStoreNodeFlags Flags);

	struct FCacheStoreNode
	{
		ILegacyCacheStore* Cache{};
		ECacheStoreFlags CacheFlags{};
		ECacheStoreNodeFlags NodeFlags{};
		TUniquePtr<ILegacyCacheStore> AsyncCache;
	};

	mutable FRWLock NodesLock;
	ECacheStoreNodeFlags CombinedNodeFlags{};
	TArray<FCacheStoreNode, TInlineAllocator<8>> Nodes;
	IMemoryCacheStore* MemoryCache;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FCacheStoreHierarchy::FCounterEvent
{
public:
	void Reset(int32 NewCount)
	{
		Count.store(NewCount, std::memory_order_relaxed);
	}

	bool Signal()
	{
		return Count.fetch_sub(1, std::memory_order_acq_rel) == 1;
	}

private:
	std::atomic<int32> Count{0};
};

class FCacheStoreHierarchy::FBatchBase
{
public:
	virtual ~FBatchBase() = default;

	void AddRef()
	{
		ReferenceCount.fetch_add(1, std::memory_order_relaxed);
	}

	void Release()
	{
		if (ReferenceCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			delete this;
		}
	}

private:
	std::atomic<int32> ReferenceCount{0};
};

template <typename PutRequestType, typename GetRequestType>
struct FCacheStoreHierarchy::TBatchParams
{
	using FPutRequest = PutRequestType;
	using FGetRequest = GetRequestType;
	using FPutResponse = decltype(DeclVal<FPutRequest>().MakeResponse(EStatus::Ok));
	using FGetResponse = decltype(DeclVal<FGetRequest>().MakeResponse(EStatus::Ok));
	using FOnPutComplete = TUniqueFunction<void (FPutResponse&& Response)>;
	using FOnGetComplete = TUniqueFunction<void (FGetResponse&& Response)>;
	using PutFunctionType = void (ILegacyCacheStore::*)(TConstArrayView<FPutRequest>, IRequestOwner&, FOnPutComplete&&);
	using GetFunctionType = void (ILegacyCacheStore::*)(TConstArrayView<FGetRequest>, IRequestOwner&, FOnGetComplete&&);
	static PutFunctionType Put();
	static GetFunctionType Get();
	static bool HasResponseData(const FGetResponse& Response);
	static bool MergeFromResponse(FGetResponse& OutResponse, const FGetResponse& Response);
	static void ModifyPolicyForResponse(decltype(FGetRequest::Policy)& Policy, const FGetResponse& Response) {}
	static FGetResponse FilterResponseByRequest(const FGetResponse& Response, const FGetRequest& Request);
	static FPutRequest MakePutRequest(const FGetResponse& Response, const FGetRequest& Request);
	static FGetRequest MakeGetRequest(const FPutRequest& Request, int32 RequestIndex);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class FCacheStoreHierarchy::ECacheStoreNodeFlags : uint32
{
	None                    = 0,

	/** This node is preceded by a node that has the Store and Local flags. */
	HasStoreLocalNode       = 1 << 0,
	/** This node is preceded by a node that has the Store and Remote flags. */
	HasStoreRemoteNode      = 1 << 1,
	/** This node is preceded by a node that has the Store and (Local or Remote) flags. */
	HasStoreNode            = HasStoreLocalNode | HasStoreRemoteNode,

	/** This node is followed by a node that has the Query and Local flags. */
	HasQueryLocalNode       = 1 << 2,
	/** This node is followed by a node that has the Query and Remote flags. */
	HasQueryRemoteNode      = 1 << 3,
	/** This node is followed by a node that has the Query and (Local or Remote) flags. */
	HasQueryNode            = HasQueryLocalNode | HasQueryRemoteNode,
};

ENUM_CLASS_FLAGS(FCacheStoreHierarchy::ECacheStoreNodeFlags);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FCacheStoreHierarchy::FCacheStoreHierarchy(IMemoryCacheStore* InMemoryCache)
	: MemoryCache(InMemoryCache)
{
	if (MemoryCache)
	{
		Add(MemoryCache, ECacheStoreFlags::Local | ECacheStoreFlags::Query | ECacheStoreFlags::StopGetStore);
	}
}

void FCacheStoreHierarchy::Add(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags)
{
	FWriteScopeLock Lock(NodesLock);
	checkf(!Algo::FindBy(Nodes, CacheStore, &FCacheStoreNode::Cache),
		TEXT("Attempting to add a cache store that was previously registered to the hierarchy."));
	TUniquePtr<ILegacyCacheStore> AsyncCacheStore(CreateCacheStoreAsync(CacheStore, Flags, MemoryCache));
	Nodes.Add({CacheStore, Flags, {}, MoveTemp(AsyncCacheStore)});
	UpdateNodeFlags();
}

void FCacheStoreHierarchy::SetFlags(ILegacyCacheStore* CacheStore, ECacheStoreFlags Flags)
{
	FWriteScopeLock Lock(NodesLock);
	FCacheStoreNode* Node = Algo::FindBy(Nodes, CacheStore, &FCacheStoreNode::Cache);
	checkf(!!Node, TEXT("Attempting to set flags on a cache store that is not registered to the hierarchy."));
	Node->CacheFlags = Flags;
	UpdateNodeFlags();
}

void FCacheStoreHierarchy::RemoveNotSafe(ILegacyCacheStore* CacheStore)
{
	FWriteScopeLock Lock(NodesLock);
	FCacheStoreNode* Node = Algo::FindBy(Nodes, CacheStore, &FCacheStoreNode::Cache);
	checkf(!!Node, TEXT("Attempting to remove a cache store that is not registered to the hierarchy."));
	Nodes.RemoveAt(UE_PTRDIFF_TO_INT32(Node - Nodes.GetData()));
	UpdateNodeFlags();
}

void FCacheStoreHierarchy::UpdateNodeFlags()
{
	ECacheStoreNodeFlags StoreFlags = ECacheStoreNodeFlags::None;
	for (int32 Index = 0, Count = Nodes.Num(); Index < Count; ++Index)
	{
		FCacheStoreNode& Node = Nodes[Index];
		Node.NodeFlags = StoreFlags;
		if (EnumHasAllFlags(Node.CacheFlags, ECacheStoreFlags::Store | ECacheStoreFlags::Local))
		{
			StoreFlags |= ECacheStoreNodeFlags::HasStoreLocalNode;
		}
		if (EnumHasAllFlags(Node.CacheFlags, ECacheStoreFlags::Store | ECacheStoreFlags::Remote))
		{
			StoreFlags |= ECacheStoreNodeFlags::HasStoreRemoteNode;
		}
	}

	ECacheStoreNodeFlags QueryFlags = ECacheStoreNodeFlags::None;
	for (int32 Index = Nodes.Num() - 1; Index >= 0; --Index)
	{
		FCacheStoreNode& Node = Nodes[Index];
		Node.NodeFlags |= QueryFlags;
		if (EnumHasAllFlags(Node.CacheFlags, ECacheStoreFlags::Query | ECacheStoreFlags::Local))
		{
			QueryFlags |= ECacheStoreNodeFlags::HasQueryLocalNode;
		}
		if (EnumHasAllFlags(Node.CacheFlags, ECacheStoreFlags::Query | ECacheStoreFlags::Remote))
		{
			QueryFlags |= ECacheStoreNodeFlags::HasQueryRemoteNode;
		}
	}

	CombinedNodeFlags = StoreFlags | QueryFlags;
}

FCacheRecordPolicy FCacheStoreHierarchy::AddPolicy(const FCacheRecordPolicy& BasePolicy, ECachePolicy Policy)
{
	return BasePolicy.Transform([Policy](ECachePolicy P) { return P | Policy; });
}

FCacheRecordPolicy FCacheStoreHierarchy::RemovePolicy(const FCacheRecordPolicy& BasePolicy, ECachePolicy Policy)
{
	return BasePolicy.Transform([Policy](ECachePolicy P) { return P & ~Policy; });
}

bool FCacheStoreHierarchy::CanQuery(const ECachePolicy Policy, const ECacheStoreFlags Flags)
{
	const ECacheStoreFlags LocationFlags =
		(EnumHasAnyFlags(Policy, ECachePolicy::QueryLocal) ? ECacheStoreFlags::Local : ECacheStoreFlags::None) |
		(EnumHasAnyFlags(Policy, ECachePolicy::QueryRemote) ? ECacheStoreFlags::Remote : ECacheStoreFlags::None);
	return EnumHasAnyFlags(Flags, LocationFlags) && EnumHasAnyFlags(Flags, ECacheStoreFlags::Query);
}

bool FCacheStoreHierarchy::CanStore(const ECachePolicy Policy, const ECacheStoreFlags Flags)
{
	const ECacheStoreFlags LocationFlags =
		(EnumHasAnyFlags(Policy, ECachePolicy::StoreLocal) ? ECacheStoreFlags::Local : ECacheStoreFlags::None) |
		(EnumHasAnyFlags(Policy, ECachePolicy::StoreRemote) ? ECacheStoreFlags::Remote : ECacheStoreFlags::None);
	return EnumHasAnyFlags(Flags, LocationFlags) && EnumHasAnyFlags(Flags, ECacheStoreFlags::Store);
}

bool FCacheStoreHierarchy::CanStoreIfOk(const ECachePolicy Policy, const ECacheStoreNodeFlags Flags)
{
	const ECacheStoreNodeFlags LocationFlags =
		(EnumHasAnyFlags(Policy, ECachePolicy::StoreLocal) ? ECacheStoreNodeFlags::HasStoreLocalNode : ECacheStoreNodeFlags::None) |
		(EnumHasAnyFlags(Policy, ECachePolicy::StoreRemote) ? ECacheStoreNodeFlags::HasStoreRemoteNode : ECacheStoreNodeFlags::None);
	return EnumHasAnyFlags(Flags, LocationFlags);
}

bool FCacheStoreHierarchy::CanQueryIfError(const ECachePolicy Policy, const ECacheStoreNodeFlags Flags)
{
	const ECacheStoreNodeFlags LocationFlags =
		(EnumHasAnyFlags(Policy, ECachePolicy::QueryLocal) ? ECacheStoreNodeFlags::HasQueryLocalNode : ECacheStoreNodeFlags::None) |
		(EnumHasAnyFlags(Policy, ECachePolicy::QueryRemote) ? ECacheStoreNodeFlags::HasQueryRemoteNode : ECacheStoreNodeFlags::None);
	return EnumHasAnyFlags(Flags, LocationFlags);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename Params>
class FCacheStoreHierarchy::TPutBatch final : public FBatchBase, public Params
{
	using FPutRequest = typename Params::FPutRequest;
	using FGetRequest = typename Params::FGetRequest;
	using FPutResponse = typename Params::FPutResponse;
	using FGetResponse = typename Params::FGetResponse;
	using FOnPutComplete = typename Params::FOnPutComplete;
	using FOnGetComplete = typename Params::FOnGetComplete;
	using Params::Put;
	using Params::Get;
	using Params::MakeGetRequest;

public:
	static void Begin(
		const FCacheStoreHierarchy& Hierarchy,
		TConstArrayView<FPutRequest> Requests,
		IRequestOwner& Owner,
		FOnPutComplete&& OnComplete);

private:
	TPutBatch(
		const FCacheStoreHierarchy& InHierarchy,
		const TConstArrayView<FPutRequest> InRequests,
		IRequestOwner& InOwner,
		FOnPutComplete&& InOnComplete)
		: Hierarchy(InHierarchy)
		, Requests(InRequests)
		, BatchOwner(InOwner)
		, OnComplete(MoveTemp(InOnComplete))
		, AsyncOwner(FPlatformMath::Min(InOwner.GetPriority(), EPriority::Highest))
	{
		AsyncOwner.KeepAlive();
		States.SetNum(Requests.Num());
	}

	void DispatchRequests();

	bool DispatchGetRequests();
	void CompleteGetRequest(FGetResponse&& Response);

	bool DispatchPutRequests();
	void CompletePutRequest(FPutResponse&& Response);

	struct FRequestState
	{
		bool bOk = false;
		bool bStop = false;
	};

	const FCacheStoreHierarchy& Hierarchy;
	TArray<FPutRequest, TInlineAllocator<1>> Requests;
	IRequestOwner& BatchOwner;
	FOnPutComplete OnComplete;

	FRequestOwner AsyncOwner;
	TArray<FRequestState, TInlineAllocator<1>> States;
	FCounterEvent RemainingRequestCount;
	int32 NodeGetIndex = -1;
	int32 NodePutIndex = 0;
};

template <typename Params>
void FCacheStoreHierarchy::TPutBatch<Params>::Begin(
	const FCacheStoreHierarchy& InHierarchy,
	const TConstArrayView<FPutRequest> InRequests,
	IRequestOwner& InOwner,
	FOnPutComplete&& InOnComplete)
{
	if (InRequests.IsEmpty() || !EnumHasAnyFlags(InHierarchy.CombinedNodeFlags, ECacheStoreNodeFlags::HasStoreNode))
	{
		return CompleteWithStatus(InRequests, InOnComplete, EStatus::Error);
	}

	TRefCountPtr<TPutBatch> State = new TPutBatch(InHierarchy, InRequests, InOwner, MoveTemp(InOnComplete));
	State->DispatchRequests();
}

template <typename Params>
void FCacheStoreHierarchy::TPutBatch<Params>::DispatchRequests()
{
	FReadScopeLock Lock(Hierarchy.NodesLock);

	for (const int32 NodeCount = Hierarchy.Nodes.Num(); NodePutIndex < NodeCount && !BatchOwner.IsCanceled(); ++NodePutIndex)
	{
		if (DispatchGetRequests() || DispatchPutRequests())
		{
			return;
		}
	}

	int32 RequestIndex = 0;
	for (const FPutRequest& Request : Requests)
	{
		const FRequestState& State = States[RequestIndex];
		if (!State.bOk && !State.bStop)
		{
			OnComplete(Request.MakeResponse(BatchOwner.IsCanceled() ? EStatus::Canceled : EStatus::Error));
		}
		++RequestIndex;
	}
}

template <typename Params>
bool FCacheStoreHierarchy::TPutBatch<Params>::DispatchGetRequests()
{
	if (NodeGetIndex >= NodePutIndex)
	{
		return false;
	}

	NodeGetIndex = NodePutIndex;

	const FCacheStoreNode& Node = Hierarchy.Nodes[NodeGetIndex];
	if (!EnumHasAnyFlags(Node.CacheFlags, ECacheStoreFlags::StopPutStore))
	{
		return false;
	}

	TArray<FGetRequest, TInlineAllocator<1>> NodeRequests;
	NodeRequests.Reserve(Requests.Num());

	int32 RequestIndex = 0;
	for (const FPutRequest& Request : Requests)
	{
		if (!States[RequestIndex].bStop && CanQuery(GetCombinedPolicy(Request.Policy), Node.CacheFlags))
		{
			NodeRequests.Add(MakeGetRequest(Request, RequestIndex));
		}
		++RequestIndex;
	}

	if (const int32 NodeRequestsCount = NodeRequests.Num())
	{
		RemainingRequestCount.Reset(NodeRequestsCount + 1);
		Invoke(Get(), Node.Cache, NodeRequests, BatchOwner,
			[State = TRefCountPtr(this)](FGetResponse&& Response)
			{
				State->CompleteGetRequest(MoveTemp(Response));
			});
		return !RemainingRequestCount.Signal();
	}

	return false;
}

template <typename Params>
void FCacheStoreHierarchy::TPutBatch<Params>::CompleteGetRequest(FGetResponse&& Response)
{
	if (Response.Status == EStatus::Ok)
	{
		const int32 RequestIndex = int32(Response.UserData);
		FRequestState& State = States[RequestIndex];
		check(!State.bStop);
		State.bStop = true;
		if (!State.bOk)
		{
			OnComplete(Requests[RequestIndex].MakeResponse(Response.Status));
		}
	}
	if (RemainingRequestCount.Signal())
	{
		DispatchRequests();
	}
}

template <typename Params>
bool FCacheStoreHierarchy::TPutBatch<Params>::DispatchPutRequests()
{
	const FCacheStoreNode& Node = Hierarchy.Nodes[NodePutIndex];
	if (!EnumHasAnyFlags(Node.CacheFlags, ECacheStoreFlags::Store))
	{
		return false;
	}

	TArray<FPutRequest, TInlineAllocator<1>> NodeRequests;
	TArray<FPutRequest, TInlineAllocator<1>> AsyncNodeRequests;

	const int32 RequestCount = Requests.Num();
	NodeRequests.Reserve(RequestCount);
	AsyncNodeRequests.Reserve(RequestCount);

	int32 RequestIndex = 0;
	for (const FPutRequest& Request : Requests)
	{
		const FRequestState& State = States[RequestIndex];
		if (!State.bStop && CanStore(GetCombinedPolicy(Request.Policy), Node.CacheFlags))
		{
			(State.bOk ? AsyncNodeRequests : NodeRequests).Add_GetRef(Request).UserData = uint64(RequestIndex);
		}
		++RequestIndex;
	}

	if (!AsyncNodeRequests.IsEmpty())
	{
		FRequestBarrier Barrier(AsyncOwner);
		Invoke(Put(), Node.AsyncCache, AsyncNodeRequests, AsyncOwner, [](auto&&){});
	}

	if (const int32 NodeRequestsCount = NodeRequests.Num())
	{
		RemainingRequestCount.Reset(NodeRequestsCount + 1);
		Invoke(Put(), Node.Cache, NodeRequests, BatchOwner,
			[State = TRefCountPtr(this)](FPutResponse&& Response)
			{
				State->CompletePutRequest(MoveTemp(Response));
			});
		return !RemainingRequestCount.Signal();
	}

	return false;
}

template <typename Params>
void FCacheStoreHierarchy::TPutBatch<Params>::CompletePutRequest(FPutResponse&& Response)
{
	if (Response.Status == EStatus::Ok)
	{
		const int32 RequestIndex = int32(Response.UserData);
		FRequestState& State = States[RequestIndex];
		check(!State.bOk && !State.bStop);
		State.bOk = true;
		Response.UserData = Requests[RequestIndex].UserData;
		OnComplete(MoveTemp(Response));
	}
	if (RemainingRequestCount.Signal())
	{
		++NodePutIndex;
		DispatchRequests();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename Params>
class FCacheStoreHierarchy::TGetBatch final : public FBatchBase, public Params
{
	using FPutRequest = typename Params::FPutRequest;
	using FGetRequest = typename Params::FGetRequest;
	using FPutResponse = typename Params::FPutResponse;
	using FGetResponse = typename Params::FGetResponse;
	using FOnPutComplete = typename Params::FOnPutComplete;
	using FOnGetComplete = typename Params::FOnGetComplete;
	using Params::Put;
	using Params::Get;
	using Params::HasResponseData;
	using Params::MergeFromResponse;
	using Params::ModifyPolicyForResponse;
	using Params::FilterResponseByRequest;
	using Params::MakePutRequest;

public:
	static void Begin(
		const FCacheStoreHierarchy& Hierarchy,
		TConstArrayView<FGetRequest> Requests,
		IRequestOwner& Owner,
		FOnGetComplete&& OnComplete);

private:
	TGetBatch(
		const FCacheStoreHierarchy& InHierarchy,
		const TConstArrayView<FGetRequest> InRequests,
		IRequestOwner& InOwner,
		FOnGetComplete&& InOnComplete)
		: Hierarchy(InHierarchy)
		, OnComplete(MoveTemp(InOnComplete))
		, Owner(InOwner)
		, AsyncOwner(FPlatformMath::Min(InOwner.GetPriority(), EPriority::Highest))
	{
		AsyncOwner.KeepAlive();
		States.Reserve(InRequests.Num());
		for (const FGetRequest& Request : InRequests)
		{
			States.Add({Request, Request.MakeResponse(EStatus::Error)});
		}
	}

	void DispatchRequests();
	void CompleteRequest(FGetResponse&& Response);

	struct FState
	{
		FGetRequest Request;
		FGetResponse Response;
		int32 NodeIndex = 0;
	};

	const FCacheStoreHierarchy& Hierarchy;
	FOnGetComplete OnComplete;
	TArray<FState, TInlineAllocator<8>> States;

	IRequestOwner& Owner;
	FRequestOwner AsyncOwner;
	FCounterEvent RemainingRequestCount;
};

template <typename Params>
void FCacheStoreHierarchy::TGetBatch<Params>::Begin(
	const FCacheStoreHierarchy& InHierarchy,
	const TConstArrayView<FGetRequest> InRequests,
	IRequestOwner& InOwner,
	FOnGetComplete&& InOnComplete)
{
	if (InRequests.IsEmpty() || !EnumHasAnyFlags(InHierarchy.CombinedNodeFlags, ECacheStoreNodeFlags::HasQueryNode))
	{
		return CompleteWithStatus(InRequests, InOnComplete, EStatus::Error);
	}

	TRefCountPtr<TGetBatch> State = new TGetBatch(InHierarchy, InRequests, InOwner, MoveTemp(InOnComplete));
	State->DispatchRequests();
}

template <typename Params>
void FCacheStoreHierarchy::TGetBatch<Params>::DispatchRequests()
{
	FReadScopeLock Lock(Hierarchy.NodesLock);

	TArray<FGetRequest, TInlineAllocator<8>> NodeRequests;
	TArray<FPutRequest, TInlineAllocator<8>> AsyncNodeRequests;

	const int32 RequestCount = States.Num();
	NodeRequests.Reserve(RequestCount);
	AsyncNodeRequests.Reserve(RequestCount);

	for (const int32 NodeCount = Hierarchy.Nodes.Num(); !Owner.IsCanceled();)
	{
		const int32 NodeIndex = Algo::MinElementBy(States, &FState::NodeIndex)->NodeIndex;
		if (NodeIndex >= NodeCount)
		{
			break;
		}

		const FCacheStoreNode& Node = Hierarchy.Nodes[NodeIndex];

		uint64 StateIndex = 0;
		for (FState& State : States)
		{
			if (NodeIndex < State.NodeIndex)
			{
				continue;
			}

			const FGetRequest& Request = State.Request;
			const FGetResponse& Response = State.Response;
			if (Response.Status == EStatus::Ok)
			{
				if (HasResponseData(Response) && CanStore(GetCombinedPolicy(Request.Policy), Node.CacheFlags))
				{
					AsyncNodeRequests.Add(MakePutRequest(Response, Request));
					++State.NodeIndex;
				}
				else if (EnumHasAnyFlags(Node.CacheFlags, ECacheStoreFlags::StopGetStore) && CanQuery(GetCombinedPolicy(Request.Policy), Node.CacheFlags))
				{
					NodeRequests.Add({Request.Name, Request.Key, AddPolicy(Request.Policy, ECachePolicy::SkipData), StateIndex});
				}
				else
				{
					++State.NodeIndex;
				}
			}
			else
			{
				if (const ECachePolicy CombinedPolicy = GetCombinedPolicy(Request.Policy); CanQuery(CombinedPolicy, Node.CacheFlags))
				{
					auto Policy = Request.Policy;
					if (CanStoreIfOk(CombinedPolicy, Node.NodeFlags))
					{
						Policy = RemovePolicy(Policy, ECachePolicy::SkipData | ECachePolicy::SkipMeta);
					}
					if (CanQueryIfError(CombinedPolicy, Node.NodeFlags))
					{
						Policy = AddPolicy(Policy, ECachePolicy::PartialRecord);
					}
					ModifyPolicyForResponse(Policy, Response);
					NodeRequests.Add({Request.Name, Request.Key, Policy, StateIndex});
				}
				else
				{
					++State.NodeIndex;
				}
			}
			++StateIndex;
		}

		if (!AsyncNodeRequests.IsEmpty())
		{
			FRequestBarrier Barrier(AsyncOwner);
			Invoke(Put(), Node.AsyncCache, AsyncNodeRequests, AsyncOwner, [](auto&&){});
			AsyncNodeRequests.Reset();
		}

		if (const int32 NodeRequestsCount = NodeRequests.Num())
		{
			RemainingRequestCount.Reset(NodeRequestsCount + 1);
			Invoke(Get(), Node.Cache, NodeRequests, Owner,
				[State = TRefCountPtr(this)](FGetResponse&& Response)
				{
					State->CompleteRequest(MoveTemp(Response));
				});
			NodeRequests.Reset();
			if (!RemainingRequestCount.Signal())
			{
				return;
			}
		}
	}

	for (FState& State : States)
	{
		if (State.Response.Status != EStatus::Ok)
		{
			OnComplete(MoveTemp(State.Response));
		}
	}
}

template <typename Params>
void FCacheStoreHierarchy::TGetBatch<Params>::CompleteRequest(FGetResponse&& Response)
{
	ON_SCOPE_EXIT
	{
		if (RemainingRequestCount.Signal())
		{
			DispatchRequests();
		}
	};

	FState& State = States[int32(Response.UserData)];
	Response.UserData = State.Request.UserData;
	const FCacheStoreNode& Node = Hierarchy.Nodes[State.NodeIndex];
	const EStatus PreviousStatus = State.Response.Status;
	const int32 PreviousNodeIndex = State.NodeIndex;

	// Failure to merge requires dispatching to the same node to try to recover missing values.
	if (!MergeFromResponse(State.Response, Response))
	{
		return;
	}

	const bool bFirstOk = State.Response.Status == EStatus::Ok && PreviousStatus == EStatus::Error;
	const bool bLastQuery = bFirstOk || !CanQueryIfError(GetCombinedPolicy(State.Request.Policy), Node.NodeFlags);

	if (bLastQuery && CanStoreIfOk(GetCombinedPolicy(State.Request.Policy), Node.NodeFlags) && HasResponseData(State.Response))
	{
		// Store any retrieved values to previous writable nodes if Ok or there are no remaining nodes to query.
		FPutRequest PutRequest = MakePutRequest(State.Response, State.Request);
		PutRequest.Policy = RemovePolicy(PutRequest.Policy, ECachePolicy::Query);
		for (int32 PutNodeIndex = 0; PutNodeIndex < PreviousNodeIndex; ++PutNodeIndex)
		{
			const FCacheStoreNode& PutNode = Hierarchy.Nodes[PutNodeIndex];
			if (CanStore(GetCombinedPolicy(State.Request.Policy), PutNode.CacheFlags))
			{
				FRequestBarrier Barrier(AsyncOwner);
				Invoke(Put(), PutNode.AsyncCache, MakeArrayView(&PutRequest, 1), AsyncOwner, [](auto&&){});
			}
		}
	}

	if (bFirstOk)
	{
		// Values may be fetched to populate earlier nodes. Remove values if requested.
		OnComplete(FilterResponseByRequest(State.Response, State.Request));
	}

	if (Response.Status == EStatus::Ok)
	{
		if (EnumHasAnyFlags(Node.CacheFlags, ECacheStoreFlags::StopGetStore))
		{
			// Never store to later nodes.
			State.Request.Policy = RemovePolicy(State.Request.Policy, ECachePolicy::Default);
		}
		else
		{
			// Never store to later remote nodes.
			// This is a necessary optimization until speculative stores have been optimized.
			State.Request.Policy = RemovePolicy(State.Request.Policy, ECachePolicy::Remote);
		}
	}

	++State.NodeIndex;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <>
auto FCacheStoreHierarchy::FCacheRecordBatchParams::Put() -> PutFunctionType
{
	return &ICacheStore::Put;
};

template <>
auto FCacheStoreHierarchy::FCacheRecordBatchParams::Get() -> GetFunctionType
{
	return &ICacheStore::Get;
};

template <>
bool FCacheStoreHierarchy::FCacheRecordBatchParams::HasResponseData(const FCacheGetResponse& Response)
{
	return Algo::AnyOf(Response.Record.GetValues(), &FValue::HasData) || Response.Record.GetMeta();
}

template <>
bool FCacheStoreHierarchy::FCacheRecordBatchParams::MergeFromResponse(
	FCacheGetResponse& OutResponse,
	const FCacheGetResponse& Response)
{
	if (OutResponse.Status == EStatus::Ok)
	{
		return true;
	}

	const FCacheRecord& NewRecord = Response.Record;
	const FCacheRecord ExistingRecord = MoveTemp(OutResponse.Record);

	OutResponse = Response;

	if (ExistingRecord.GetValues().IsEmpty())
	{
		return true;
	}

	bool bOk = true;
	FCacheRecordBuilder Builder(ExistingRecord.GetKey());
	Builder.SetMeta(CopyTemp(NewRecord.GetMeta() ? NewRecord.GetMeta() : ExistingRecord.GetMeta()));
	for (const FValueWithId& Value : NewRecord.GetValues())
	{
		if (Value.HasData())
		{
			Builder.AddValue(Value);
		}
		else if (const FValueWithId& ExistingValue = ExistingRecord.GetValue(Value.GetId()); ExistingValue && ExistingValue.HasData())
		{
			if (Value == ExistingValue)
			{
				// There is a matching existing value with data. Merge it into the new record.
				Builder.AddValue(ExistingValue);
			}
			else
			{
				// There is an existing value and it differs. Requery the current node for this value.
				Builder.AddValue(Value);
				OutResponse.Status = EStatus::Error;
				bOk = false;
			}
		}
		else
		{
			Builder.AddValue(Value);
		}
	}
	OutResponse.Record = Builder.Build();
	return bOk;
}

template <>
void FCacheStoreHierarchy::FCacheRecordBatchParams::ModifyPolicyForResponse(
	FCacheRecordPolicy& Policy,
	const FCacheGetResponse& Response)
{
	const TConstArrayView<FValueWithId> Values = Response.Record.GetValues();
	if (Values.IsEmpty())
	{
		return;
	}

	FCacheRecordPolicyBuilder Builder(Policy.GetBasePolicy());

	// Skip values that are present in the response.
	for (const FValueWithId& Value : Values)
	{
		if (Value.HasData())
		{
			Builder.AddValuePolicy(Value.GetId(), ECachePolicy::None);
		}
	}

	// Copy any the policy for any other values from the source policy.
	for (const FCacheValuePolicy& ValuePolicy : Policy.GetValuePolicies())
	{
		if (!Response.Record.GetValue(ValuePolicy.Id).HasData())
		{
			Builder.AddValuePolicy(ValuePolicy);
		}
	}

	Policy = Builder.Build();
}

template <>
FCacheGetResponse FCacheStoreHierarchy::FCacheRecordBatchParams::FilterResponseByRequest(
	const FCacheGetResponse& Response,
	const FCacheGetRequest& Request)
{
	const ECachePolicy RecordPolicy = Request.Policy.GetRecordPolicy();
	const bool bMightSkipData = EnumHasAnyFlags(RecordPolicy, ECachePolicy::SkipData) || !Request.Policy.IsUniform();
	if ((bMightSkipData && Algo::AnyOf(Response.Record.GetValues(), &FValue::HasData)) ||
		(EnumHasAnyFlags(RecordPolicy, ECachePolicy::SkipMeta) && Response.Record.GetMeta()))
	{
		FCacheRecordBuilder Builder(Response.Record.GetKey());
		if (!EnumHasAnyFlags(RecordPolicy, ECachePolicy::SkipMeta))
		{
			Builder.SetMeta(CopyTemp(Response.Record.GetMeta()));
		}
		for (const FValueWithId& Value : Response.Record.GetValues())
		{
			if (EnumHasAnyFlags(Request.Policy.GetValuePolicy(Value.GetId()), ECachePolicy::SkipData))
			{
				Builder.AddValue(Value.GetId(), Value.RemoveData());
			}
			else
			{
				Builder.AddValue(Value);
			}
		}
		return {Response.Name, Builder.Build(), Response.UserData, Response.Status};
	}
	return Response;
}

template <>
FCachePutRequest FCacheStoreHierarchy::FCacheRecordBatchParams::MakePutRequest(
	const FCacheGetResponse& Response,
	const FCacheGetRequest& Request)
{
	FCacheRecordPolicy Policy = Request.Policy;
	if (!Algo::AllOf(Response.Record.GetValues(), &FValue::HasData) &&
		!EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::PartialRecord))
	{
		Policy = Policy.Transform([](ECachePolicy P) { return P | ECachePolicy::PartialRecord; });
	}
	return {Response.Name, Response.Record, MoveTemp(Policy)};
}

template <>
FCacheGetRequest FCacheStoreHierarchy::FCacheRecordBatchParams::MakeGetRequest(
	const FCachePutRequest& Request,
	const int32 RequestIndex)
{
	return {Request.Name, Request.Record.GetKey(), AddPolicy(Request.Policy, ECachePolicy::SkipData), uint64(RequestIndex)};
}

void FCacheStoreHierarchy::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	TPutBatch<FCacheRecordBatchParams>::Begin(*this, Requests, Owner, MoveTemp(OnComplete));
}

void FCacheStoreHierarchy::Get(
	const TConstArrayView<FCacheGetRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetComplete&& OnComplete)
{
	TGetBatch<FCacheRecordBatchParams>::Begin(*this, Requests, Owner, MoveTemp(OnComplete));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <>
auto FCacheStoreHierarchy::FCacheValueBatchParams::Put() -> PutFunctionType
{
	return &ICacheStore::PutValue;
};

template <>
auto FCacheStoreHierarchy::FCacheValueBatchParams::Get() -> GetFunctionType
{
	return &ICacheStore::GetValue;
};

template <>
bool FCacheStoreHierarchy::FCacheValueBatchParams::HasResponseData(const FCacheGetValueResponse& Response)
{
	return Response.Value.HasData();
}

template <>
bool FCacheStoreHierarchy::FCacheValueBatchParams::MergeFromResponse(
	FGetResponse& OutResponse,
	const FGetResponse& Response)
{
	if (OutResponse.Status == EStatus::Error)
	{
		OutResponse = Response;
	}
	return true;
}

template <>
FCacheGetValueResponse FCacheStoreHierarchy::FCacheValueBatchParams::FilterResponseByRequest(
	const FCacheGetValueResponse& Response,
	const FCacheGetValueRequest& Request)
{
	if (Response.Value.HasData() && EnumHasAnyFlags(Request.Policy, ECachePolicy::SkipData))
	{
		return {Response.Name, Response.Key, Response.Value.RemoveData(), Response.UserData, Response.Status};
	}
	return Response;
}

template <>
FCachePutValueRequest FCacheStoreHierarchy::FCacheValueBatchParams::MakePutRequest(
	const FCacheGetValueResponse& Response,
	const FCacheGetValueRequest& Request)
{
	return {Response.Name, Response.Key, Response.Value, Request.Policy};
}

template <>
FCacheGetValueRequest FCacheStoreHierarchy::FCacheValueBatchParams::MakeGetRequest(
	const FCachePutValueRequest& Request,
	const int32 RequestIndex)
{
	return {Request.Name, Request.Key, AddPolicy(Request.Policy, ECachePolicy::SkipData), uint64(RequestIndex)};
}

void FCacheStoreHierarchy::PutValue(
	const TConstArrayView<FCachePutValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutValueComplete&& OnComplete)
{
	TPutBatch<FCacheValueBatchParams>::Begin(*this, Requests, Owner, MoveTemp(OnComplete));
}

void FCacheStoreHierarchy::GetValue(
	TConstArrayView<FCacheGetValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetValueComplete&& OnComplete)
{
	TGetBatch<FCacheValueBatchParams>::Begin(*this, Requests, Owner, MoveTemp(OnComplete));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FCacheStoreHierarchy::FGetChunksBatch final : public FBatchBase
{
public:
	static void Begin(
		const FCacheStoreHierarchy& Hierarchy,
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete);

private:
	FGetChunksBatch(
		const FCacheStoreHierarchy& InHierarchy,
		const TConstArrayView<FCacheGetChunkRequest> InRequests,
		IRequestOwner& InOwner,
		FOnCacheGetChunkComplete&& InOnComplete)
		: Hierarchy(InHierarchy)
		, OnComplete(MoveTemp(InOnComplete))
		, Owner(InOwner)
	{
		States.Reserve(InRequests.Num());
		for (const FCacheGetChunkRequest& Request : InRequests)
		{
			States.Add({Request});
		}
	}

	void DispatchRequests();
	void CompleteRequest(FCacheGetChunkResponse&& Response);

	struct FState
	{
		FCacheGetChunkRequest Request;
		EStatus Status = EStatus::Error;
	};

	const FCacheStoreHierarchy& Hierarchy;
	FOnCacheGetChunkComplete OnComplete;
	TArray<FState, TInlineAllocator<8>> States;

	IRequestOwner& Owner;
	FCounterEvent RemainingRequestCount;
	int32 NodeIndex = 0;
};

void FCacheStoreHierarchy::FGetChunksBatch::Begin(
	const FCacheStoreHierarchy& InHierarchy,
	const TConstArrayView<FCacheGetChunkRequest> InRequests,
	IRequestOwner& InOwner,
	FOnCacheGetChunkComplete&& InOnComplete)
{
	if (InRequests.IsEmpty() || !EnumHasAnyFlags(InHierarchy.CombinedNodeFlags, ECacheStoreNodeFlags::HasQueryNode))
	{
		return CompleteWithStatus(InRequests, InOnComplete, EStatus::Error);
	}

	TRefCountPtr<FGetChunksBatch> State = new FGetChunksBatch(InHierarchy, InRequests, InOwner, MoveTemp(InOnComplete));
	State->DispatchRequests();
}

void FCacheStoreHierarchy::FGetChunksBatch::DispatchRequests()
{
	FReadScopeLock Lock(Hierarchy.NodesLock);

	TArray<FCacheGetChunkRequest, TInlineAllocator<8>> NodeRequests;
	NodeRequests.Reserve(States.Num());

	for (const int32 NodeCount = Hierarchy.Nodes.Num(); NodeIndex < NodeCount && !Owner.IsCanceled(); ++NodeIndex)
	{
		const FCacheStoreNode& Node = Hierarchy.Nodes[NodeIndex];

		uint64 StateIndex = 0;
		for (const FState& State : States)
		{
			const FCacheGetChunkRequest& Request = State.Request;
			if (State.Status == EStatus::Error && CanQuery(Request.Policy, Node.CacheFlags))
			{
				NodeRequests.Add_GetRef(Request).UserData = StateIndex;
			}
			++StateIndex;
		}

		if (const int32 NodeRequestsCount = NodeRequests.Num())
		{
			RemainingRequestCount.Reset(NodeRequestsCount + 1);
			Node.Cache->GetChunks(NodeRequests, Owner,
				[State = TRefCountPtr(this)](FCacheGetChunkResponse&& Response)
				{
					State->CompleteRequest(MoveTemp(Response));
				});
			NodeRequests.Reset();
			if (!RemainingRequestCount.Signal())
			{
				return;
			}
		}
	}

	for (const FState& State : States)
	{
		if (State.Status != EStatus::Ok)
		{
			OnComplete(State.Request.MakeResponse(Owner.IsCanceled() ? EStatus::Canceled : EStatus::Error));
		}
	}
}

void FCacheStoreHierarchy::FGetChunksBatch::CompleteRequest(FCacheGetChunkResponse&& Response)
{
	FState& State = States[int32(Response.UserData)];
	if (Response.Status == EStatus::Ok)
	{
		check(State.Status == EStatus::Error);
		Response.UserData = State.Request.UserData;
		OnComplete(MoveTemp(Response));
	}
	State.Status = Response.Status;

	if (RemainingRequestCount.Signal())
	{
		++NodeIndex;
		DispatchRequests();
	}
}

void FCacheStoreHierarchy::GetChunks(
	TConstArrayView<FCacheGetChunkRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetChunkComplete&& OnComplete)
{
	FGetChunksBatch::Begin(*this, Requests, Owner, MoveTemp(OnComplete));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FCacheStoreHierarchy::LegacyStats(FDerivedDataCacheStatsNode& OutNode)
{
	FReadScopeLock Lock(NodesLock);
	OutNode.Children.Reserve(Nodes.Num());
	for (const FCacheStoreNode& Node : Nodes)
	{
		Node.Cache->LegacyStats(OutNode.Children.Add_GetRef(MakeShared<FDerivedDataCacheStatsNode>()).Get());
	}
}

bool FCacheStoreHierarchy::LegacyDebugOptions(FBackendDebugOptions& Options)
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ILegacyCacheStore* CreateCacheStoreHierarchy(ICacheStoreOwner*& OutOwner, IMemoryCacheStore* MemoryCache)
{
	FCacheStoreHierarchy* Hierarchy = new FCacheStoreHierarchy(MemoryCache);
	OutOwner = Hierarchy;
	return Hierarchy;
}

} // UE::DerivedData
