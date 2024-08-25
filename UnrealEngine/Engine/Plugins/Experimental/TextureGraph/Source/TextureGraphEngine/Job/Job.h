// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Helper/Promise.h"
#include "Device/DeviceNativeTask.h"
#include "JobCommon.h"
#include "JobArgs.h"
#include "Data/TiledBlob.h"

class Job;
class Scheduler;
class JobBatch;
class MixUpdateCycle;

DECLARE_LOG_CATEGORY_EXTERN(LogJob, Log, Verbose);

class RenderMesh;

//////////////////////////////////////////////////////////////////////////
class TEXTUREGRAPHENGINE_API Job : public DeviceNativeTask
{
public:
	struct JobStats
	{
		double						BeginNativeTime = 0.0;		/// The timestamp [ms] when this job started
		double						EndNativeTime = 0.0;		/// The timestamp [ms] when this job ended
		double						BeginRunTime = 0.0;			/// The timestamp [ms] when this job RUN is started	
		double						EndRunTime = 0.0;			/// The timestamp [ms] when this job RUN has ended

		double						TargetPrepStartTime = 0;	/// The timestamp [ms] when this job PrepareTargets was run
		double						TargetPrepEndTime = 0;		/// The timestamp [ms] when this job PrepareTargets has ended
		double						TargetPrepWaitStartTime = 0;/// The timestamp [ms] when this job started waiting for targets to be prepared (all promises to return from Device)
		double						TargetPrepWaitEndTime = 0;	/// The timestamp [ms] when this job when the waiting of promises has finished
	};

protected:
	typedef T_Tiles<JobResultPtr>	JobResultPtrTiles;

	UMixInterface*					MixObj = nullptr;	/// The mix that this job belongs to
	int64							Id = -1;			/// Unique ID for the job [-1 means it was unassigned]
	int32							QueueId = -1;		/// ID of the queue that this job belongs to
	TArray<JobArgPtr>				Args;				/// The arguments for the job
	BlobTransformPtr				Transform;			/// The transformation that we'll be doing for the job
	FWeakObjectPtr					ErrorOwner;			/// Error owner is an object associated with any error reported to the TextureGraphEngine::ErrorReporter during this job execution
	mutable CHashPtr				HashValue = nullptr;/// What is the hash of this job
	TiledBlob_PromisePtr			ResultOrg;			/// The original result that was created
	TiledBlobRef					Result;				/// The rendering result of this job
	BufferDescriptorPtr				DesiredResultDesc;	/// Desired descriptor for the result
	JobResultPtrTiles				TileResults;		/// The results for all the tiles
	JobResultPtr					FinalJobResult;		/// The overall result info for this job

	JobPArray						Parents;			/// [upstream] Jobs that this job is dependent on
	JobPtrArray						Children;			/// [downstream] Jobs that are dependent on this job

	bool							bIsTiled = true;	/// Whether the job runs in tiled mode or not. You can override this and force the job to run in non-tiled mode
	int32							TargetId = -1;		/// What is our target

	TileInvalidateMatrix			TileInvalidationMatrix; /// The tiles that were actually invalidated

	BufferDescriptor				TileDesc;			/// The descriptor for each tile

	FCriticalSection				Mutex;				/// For preventing shared table writes
	JobRunInfo						RunInfo;			/// The run info that was saved when it was passed as argument

	JobStats						Stats;				/// Job timing stats collected during execution

	const RenderMesh*				Mesh = nullptr;	/// The mesh that we're targeting 

	int32							ReplayCount = 0;	/// Counting the number of debug replay of this job
	bool							bIsNoCache = false;	/// Whether we should ignore the cache

	JobPtrVec						JobsGeneratedPrior;	/// [Prior] The jobs that were generated from this job
	JobPtrVec						JobsGeneratedAfter;	/// [After] The jobs that were generated from this job

	JobPtrW							Generator;			/// The job that generated this job. This is useful in certain situations
	FString							DebugJobName;		/// The name of the job

	//std::vector<std::shared_ptr<cti::promise<TiledBlobPtr>>> _blobPreparedPromises;

	typedef std::pair<int32, int32>	IndexPair;
	typedef TMap<HashType, IndexPair> JobLUT;

	/// Binds/Unbinds args for individual tiles or the entire blob if its a non-tiled job
	virtual AsyncJobArgResultPtr	BindArgs(JobRunInfo NewRunInfo, BlobTransformPtr TransformObj, int32 TileX, int32 TileY);
	virtual AsyncJobArgResultPtr	UnbindArgs(JobRunInfo NewRunInfo, BlobTransformPtr TransformObj, int32 TileX, int32 TileY);

	/// Binds/Unbinds args for all the tiles at once and returns a promise
	virtual AsyncInt				BindArgs_All(JobRunInfo RunInfo);
	virtual AsyncInt				UnbindArgs_All(JobRunInfo RunInfo);

	typedef std::function			<AsyncJobArgResultPtr(JobRunInfo, BlobTransformPtr, int32, int32)> Bind_Unbind_Func;
	virtual AsyncInt				Bind_Or_Unbind_All_Generic(JobRunInfo NewRunInfo, Bind_Unbind_Func BFunc);

	virtual AsyncJobResultPtr		RunTile(JobRunInfo RunInfo, int32 TileX, int32 TileY);
	virtual AsyncJobResultPtr		RunSingle(JobRunInfo RunInfo);
	virtual AsyncJobResultPtr		FinaliseTiles(JobRunInfo RunInfo);
	void							SetTileResult(int32 TileX, int32 TileY, JobResultPtr TileResult);
	virtual AsyncPrepareResult		PrepareTargets(JobBatch* batch);

	AsyncTransformResultPtr			ExecTransform(JobRunInfo RunInfo, BlobTransformPtr TransformObj , int32 TileX, int32 TileY, CHashPtr jobHash);
	virtual void					MarkJobDone();
	virtual bool					IsDiscard() const;

	//////////////////////////////////////////////////////////////////////////
	/// These methods can be used to execute a Job then and there
	//////////////////////////////////////////////////////////////////////////
	virtual AsyncJobResultPtr		Run(JobRunInfo RunInfo);
	virtual AsyncPrepareResult		PrepareResources(JobBatch* batch);

	virtual bool					CheckCached();
	BufferDescriptor				GetResultDesc() const;
	BufferDescriptor				GetCombinedDesc(BufferDescriptor& ArgsDescCombined, size_t& Count) const;

public:
									Job(int32 TargetId, BlobTransformPtr InTransform, UObject* ErrorOwner = nullptr, uint16 InPriority = (uint16)E_Priority::kNormal, uint64 InId = -1);
									Job(UMixInterface* InMix, int32 TargetId, BlobTransformPtr Transform, UObject* ErrorOwner = nullptr, uint16 priority = (uint16)E_Priority::kNormal, uint64 id = -1);
	virtual							~Job() override;


	virtual Job*					SetArgs(const TArray<JobArgPtr>& NewArgs);
	virtual Job*					AddArg(JobArgPtr Arg);
	uint32_t						NumArgs() const;
	JobArgPtr						GetArg(uint32_t Index) const;
	virtual void					AddResultToBlobber();

	virtual CHashPtr				Hash() const;
	virtual CHashPtr				TileHash(int32 TileX, int32 TileY) const;
	virtual bool					CanHandleTiles() const;
	virtual bool					CheckDefaultArgs() const;

	virtual TiledBlobPtr			InitResult(FString NewName, const BufferDescriptor* InDesiredDesc, int32 NumTilesX = 0, int32 NumTilesY = 0);

	void							ResetForReplay(bool noCache); /// For debug purpose, reset the job so it can be replayed

	//////////////////////////////////////////////////////////////////////////
	/// These methods are used to queue up the job as a DeviceNativeTask.
	/// You cannot call the DeviceNativeTask implementation directly. That is
	/// done by the Device itself.
	//////////////////////////////////////////////////////////////////////////
	virtual AsyncInt				BeginNative(JobRunInfo RunInfo);
	virtual AsyncJobResultPtr		EndNative();

	//////////////////////////////////////////////////////////////////////////
	/// BEGIN: DeviceNativeTask implementation
	//////////////////////////////////////////////////////////////////////////
protected:
	virtual TiledBlobPtr			InitLateBoundResult(FString NewName, BufferDescriptor DesiredDesc, size_t NumInputBlobs);

	virtual AsyncInt				PreExecAsync(ENamedThreads::Type execThread, ENamedThreads::Type ReturnThread) override;
	virtual int32					Exec() override;
	virtual void					PostExec() override;
	virtual AsyncInt				ExecAsync(ENamedThreads::Type execThread, ENamedThreads::Type ReturnThread) override;
	virtual JobPtrVec				GetArgDependencies();

public:
	virtual Device*					GetTargetDevice() const override;
	virtual ENamedThreads::Type		GetExecutionThread() const override;
	virtual bool					IsAsync() const override;
	bool							GetTileInvalidation(int32 TileX, int32 TileY) const;

	virtual void					GetDependencies(JobPtrVec& Prior, JobPtrVec& After, JobRunInfo RunInfo);

	virtual FString					GetName() const override;

	virtual bool					DebugCompleteCheck() override;

	//////////////////////////////////////////////////////////////////////////
	/// END: DeviceNativeTask implementation
	//////////////////////////////////////////////////////////////////////////
	virtual bool					CheckCulled(JobRunInfo RunInfo);
	virtual FString					GetRunTimings(double BatchStartTime) const;
	virtual FString					GetDebugName() const override;

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE int64				GetJobId() const { return Id; }
	FORCEINLINE void				SetJobId(int64 NewJobId) { Id = NewJobId; }
	FORCEINLINE int32				GetQueueId() const { return QueueId; }
	FORCEINLINE void				SetQueueId(int32 NewQueueId) { QueueId = NewQueueId; }
	FORCEINLINE BlobTransformPtr	GetTransform() const { return Transform; }
	FORCEINLINE TiledBlobRef		GetResult() const { return Result; }

	FORCEINLINE bool				GetTiled() const { return bIsTiled; }
	FORCEINLINE Job*				SetTiled(bool bTiled) { bIsTiled = bTiled; return this; }

	FORCEINLINE JobStats			GetStats() const { return Stats; }

	FORCEINLINE const JobRunInfo&	GetRunInfo() const { return RunInfo; }
	FORCEINLINE const RenderMesh*	GetMesh() const { return Mesh; }
	FORCEINLINE void				SetMesh(RenderMesh* NewMesh) { Mesh = NewMesh; }
	FORCEINLINE JobResultPtr		GetTileResult(int32 TileX, int32 TileY) const
	{
		if (TileX >= 0 && TileY >= 0)
			return TileResults[TileX][TileY];
		return FinalJobResult;
	}

	FORCEINLINE int32				GetReplayCount() const { return ReplayCount; } /// For debug purpose, report the replay number
	FORCEINLINE int32				GetTargetId() const { return TargetId; }

	FORCEINLINE JobPtrVec&			GeneratedPriorJobs() { return JobsGeneratedPrior; }
	FORCEINLINE JobPtrVec&			GeneratedAfterJobs() { return JobsGeneratedAfter; }
	FORCEINLINE const JobPtrW&		GeneratorJob() const { return Generator; }

	FORCEINLINE UMixInterface*		GetMix() const { return MixObj; }
	FORCEINLINE void				SetMix(UMixInterface* mix) { MixObj = mix; }

	FORCEINLINE BlobRef				GetResultRef() const { return BlobRef(std::static_pointer_cast<Blob>(Result.get()), false); }
	FORCEINLINE TiledBlob_PromisePtr GetResultPromise() const { return std::static_pointer_cast<TiledBlob_Promise>(Result.get()); }

	FORCEINLINE UObject*			GetErrorOwner() const { return ErrorOwner.Get(); }
};

//////////////////////////////////////////////////////////////////////////
