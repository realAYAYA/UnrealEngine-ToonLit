// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/LruCache.h"
#include "CoreMinimal.h"
#include "Data/BinCache.h"
#include "Data/RawBuffer.h"
#include "DeviceBuffer.h"
#include "DeviceType.h"
#include "Helper/Promise.h"
#include "TaskPriorityQueue.h"
#include <list>
#include <memory>
#include <vector>
#include <queue>


class Blob;
class DeviceNativeTask;
typedef std::shared_ptr<DeviceNativeTask>	DeviceNativeTaskPtr;
typedef std::weak_ptr<DeviceNativeTask>		DeviceNativeTaskPtrW;

struct JobResult;
typedef std::shared_ptr<JobResult>			JobResultPtr;
typedef cti::continuable<JobResultPtr>		AsyncJobResultPtr;

class DeviceObserverSource;
typedef std::shared_ptr<DeviceObserverSource> DeviceObserverSourcePtr;

using AsyncBool = cti::continuable<bool>;

DECLARE_LOG_CATEGORY_EXTERN(LogDevice, Log, Verbose);


struct CombineSplitArgs
{
	DeviceBufferRef					Buffer;						/// Main buffer that should be used for combining to or split from
	const T_Tiles<DeviceBufferRef>& Tiles;						/// Tiles that needs to be merged or split
	bool							IsArray = false;			/// if operating buffer should be treated as Array 
};

//////////////////////////////////////////////////////////////////////////
/// Represents a general compute device that is capable of running some
/// computation on a blob.
//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API Device
{
	friend class					DeviceBuffer;
	friend class					Blob;
	friend class					Blobber;

public:
	DECLARE_EVENT(Device, Event_ResourceUpdate);

	Event_ResourceUpdate			ResourceUpdateEvent;			/// Triggered when history is updated
	
	static FString					DeviceType_Names(DeviceType DevType);

	typedef BinCache<HashType, DeviceBufferPtrW> BufferCache;
	typedef std::vector<DeviceBuffer*> DeviceBufferPList;
	typedef std::vector<DeviceNativeTaskPtr> DeviceNativeTaskPtrVec;
	typedef std::vector<DeviceNativeTaskPtrW> DeviceNativeTaskPtrWVec;

protected:
	friend class DeviceManager;

	static const double				PrintStatsInterval;		/// Print stats every <this many> milliseconds
	static const double				DefaultBufferTransferInternal; /// Buffer transfers are updated every few milliseconds represented by this variable

	static const float				DefaultMaxCacheConsumptionBeforeCollection;
	static const float				DefaultMinCacheConsumptionBeforeCollection;
	static const uint64				DefaultMaxBatchDeltaDefault;
	static const double				DefaultMinLastUsageDuration;

	//////////////////////////////////////////////////////////////////////////
	float							MaxCacheConsumptionBeforeCollection 
										= DefaultMaxCacheConsumptionBeforeCollection;	/// Maximum allowed consumption (percentage) of the cache before collection. 
																						/// The cache will keep on collecting until current consumption goes below max consumption

	float							MinCacheConsumptionBeforeCollection 
										= DefaultMinCacheConsumptionBeforeCollection;	/// Minimum allowed consumption (percentage) of the cache before collection. 
																						/// The cache will not start collecting until this amount of consumption is hit

	uint64							MaxBatchDeltaDefault = DefaultMaxBatchDeltaDefault;	/// Maximum batch delta allow before we start collecting the device
	double							MinLastUsageDuration = DefaultMinLastUsageDuration;	/// A DeviceBuffer must NOT have been used for a certain time before it can be collected
																						/// by the Device. This represents that duration in milliseconds

	//////////////////////////////////////////////////////////////////////////
	BufferCache						Cache;							/// The buffer cache

	DeviceBufferPList				GCTargets;						/// Items that are prime targets for garbage collection
	mutable FCriticalSection		GCTargetsLock;					/// Lock for GCTargets 

	size_t							MaxMemory = 0;					/// The max memory available to us
	size_t							MaxThreads = 1;					/// Max concurrent threads
	DeviceType						Type = DeviceType::Count;		/// What is the type of this device
	DeviceBuffer*					BufferFactory = nullptr;		/// The device buffer factory
	double							TimeStatsPrinted = 0;			/// The last time when stats were printed
	bool							ShouldPrintStats = false;		/// Whether to print stats all all

	size_t							MemUsed = 0;					/// How much main memory are we using 
	size_t							MemUsedNative = 0;				/// How much native memory are we using (this EXCLUDES main memory so that its counted twice)

	mutable FCriticalSection		NativeTaskLock;					/// Lock for _nativeTasks vector
	DeviceNativeTask_PriorityQueueStrong NativeTasks;				/// The tasks that are queued up to be executed over here
	DeviceNativeTaskPtrVec			FinishedNativeTasks;			/// The tasks that are finished. We clear this queue on the game thread

	DeviceNativeTaskPtr				CurrentWaitTask;				/// The task that we're currently waiting on (for debugging purposes only)

	std::atomic_bool				ShouldTerminate = false;		/// Whether the device is getting terminated or not

	DeviceNativeTask_PriorityQueue	NativePreExecWait;				/// Tasks queued in the pre-exec queue. This runs in a separate thread and makes sure that
																	/// all the previous dependencies of a particular task have been finished

	DeviceNativeTask_PriorityQueue	NativePreExec;					/// Tasks queued in the pre-exec queue. These are executed on the game thread
	std::unique_ptr<std::thread>	PreExecThread = nullptr;		/// Thread running pre-exec
	size_t							PreExecThreadId = 0;			/// The thread id of the pre-exec thread (used mostly for debugging and error checking)


	DeviceNativeTask_PriorityQueue	NativeExec;						/// Tasks queued in the exec queue
	std::unique_ptr<std::thread>	ExecThread = nullptr;			/// Thread running exec
	size_t							ExecThreadId = 0;				/// The thread id of the exec thread (used mostly for debugging and error checking)
	ENamedThreads::Type				ExecThreadType					= ENamedThreads::AnyBackgroundThreadNormalTask;
																	/// The thread on which the exec queue will run

	DeviceNativeTask_PriorityQueue	NativePostExec;					/// Tasks queued in the post-exec queue
	std::unique_ptr<std::thread>	PostExecThread = nullptr;		/// Thread running post-exec
	size_t							PostExecThreadId = 0;			/// The thread id of the post-exec thread (used mostly for debugging and error checking)

	DeviceObserverSourcePtr			ObserverSource;					/// Device observer source that sends of different events happening within the device

	mutable std::mutex				AwaitingCollectionMutex;		/// Mutex for the list below
	DeviceBufferPList				AwaitingCollection;				/// Device buffers that are awaiting collection

	virtual void					UpdateAwaitingCollection();

									Device(DeviceType InType, DeviceBuffer* InBufferFactory);
	virtual							~Device();

	// Method to wait for all the queued tasks. 
	// Can be extremely slow and should not
	// be used outside of tests
	virtual AsyncBool				WaitForQueuedTasks(ENamedThreads::Type ReturnThread = ENamedThreads::UnusedAnchor);
	virtual	void					CallDeviceUpdate();
	virtual void					GarbageCollect();

	virtual void					UpdatePreExec();

	virtual bool					ShouldCollect(double TimeNow, DeviceBuffer* Buffer);
	virtual AsyncJobResultPtr		UpdateDeviceTransfers();

	/// To be called from DeviceBuffer only (on destruction)
public:
	static void						CollectBuffer(DeviceBuffer* Buffer);

protected:
	virtual void					Collect(DeviceBuffer* Buffer);

	DeviceBufferRef					AddNewRef_Internal(DeviceBuffer* Buffer);
	virtual DeviceBufferRef			AddInternal(DeviceBufferRef& Buffer);
	virtual void					RemoveInternal(DeviceBuffer* Buffer);
	virtual void					Touch(HashType Hash);
	virtual void					PrintStats();

	virtual void					Terminate();

	virtual void					UpdateNative();
	virtual void					PreExecThreadFunc();
	virtual void					ExecThreadFunc();
	virtual void					PostExecThreadFunc();
	virtual void					MarkTaskFinished(DeviceNativeTaskPtr& Task, bool bRemoveFromOwnerQueue);

	/// Flushes a specific number of tasks from the _nativeTasks vector.
	/// Set maxCount = 0 to run all the tasks currently queued up. This
	/// function is thread safe.
	/// maxCount: Maximum number of tasks to run
	/// minCount: Minimum number of tasks to run. This is ude in conjunction with timeInterval
	/// timeInterval (milliseconds): Run as many minCount tasks (in batches) 
	/// until the timeInterval is expired or the queue has been emptied
	//virtual void					FlushNativeTasks(size_t maxCount, size_t minCount, double timeInterval);
	//virtual DeviceNativeTaskPtrVec	FetchNativeTasks(size_t maxCount, size_t minCount);
	//virtual void					FlushNativeTasks_Internal(DeviceNativeTaskPtrVec& tasks);

	virtual void					Free();

public:
	virtual DeviceBufferRef			Create(RawBufferPtr Raw);
	virtual DeviceBufferRef			Create(BufferDescriptor Desc, CHashPtr Hash);

	virtual AsyncDeviceBufferRef	CombineFromTiles(const CombineSplitArgs& CombineArgs);
	virtual AsyncDeviceBufferRef	SplitToTiles(const CombineSplitArgs& SplitArgs);


	/// Acquire the device for use
	virtual cti::continuable<int32>	Use() const;

	virtual size_t					GetMaxMemory() const;
	virtual size_t					GetMaxThreads() const;

	virtual DeviceBufferRef			CreateCompatible_Copy(DeviceBufferRef source);
	virtual AsyncDeviceBufferRef	Transfer(DeviceBufferRef source);
	virtual DeviceBufferRef			Find(HashType Hash, bool Touch);
	virtual void					Update(float dt);
	virtual AsyncJobResultPtr		UpdateIdle();

	virtual FString					Name() const { return "Generic"; }
	
	/// Adds a new task to the _nativeTasks vector. Thread-safe
	virtual void					AddNativeTask(DeviceNativeTaskPtr Task);

	virtual void					TransferAborted(DeviceBufferRef Buffer);

	using							Visitor = std::function<void (const std::shared_ptr<DeviceBuffer>&, uint32)>; /// Visitor callable receiving the bufferDevice visited and the index in the lru Cache
	uint32							TraverseBufferCache(Visitor visitor);

	void							RegisterObserverSource(const DeviceObserverSourcePtr& observerSource);
	bool							CanFree() const;
	virtual void					ClearCache();

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE bool				IsMultithreaded() const { return GetMaxThreads() > 1; }
	FORCEINLINE bool				GetShouldPrintStats() const { return ShouldPrintStats; }
	FORCEINLINE void				SetShouldPrintStats(bool Value) { ShouldPrintStats = Value; }
	FORCEINLINE DeviceType			GetType() const { return Type; }
	FORCEINLINE uint64				GetMemUsed() const { return MemUsed; }
	FORCEINLINE uint64				GetNumDeviceBuffersUsed() const { return Cache.GetCache().Num(); }
	FORCEINLINE uint64				GetNumDeviceBuffersMax() const { return Cache.GetCache().Max(); }

	FORCEINLINE DeviceObserverSourcePtr GetObserverSource() const { return ObserverSource; }
	FORCEINLINE size_t				GetPreExecThreadId() const { return PreExecThreadId; }
	FORCEINLINE size_t				GetExecThreadId() const { return ExecThreadId; }
	FORCEINLINE size_t				GetPostExecThreadId() const { return PostExecThreadId; }

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static AsyncDeviceBufferRef		SplitToTiles_Generic(const CombineSplitArgs& splitArgs);

	//////////////////////////////////////////////////////////////////////////
	/// Events
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE Event_ResourceUpdate& E_ResourceUpdate() { return ResourceUpdateEvent; }
};
