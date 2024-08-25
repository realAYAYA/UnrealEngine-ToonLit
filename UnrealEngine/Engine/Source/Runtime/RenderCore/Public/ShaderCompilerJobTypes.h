// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformTime.h"
#include "Hash/Blake3.h"
#include "Shader.h"
#include "ShaderCompilerCore.h"
#include "ShaderPreprocessTypes.h"
#include "VertexFactory.h"
#include "Templates/Function.h"

class FShaderCommonCompileJob;
class FShaderCompileJob;
class FShaderPipelineCompileJob;

namespace UE::DerivedData { class FRequestOwner; }

/** Results for a single compiled shader map. */
struct FShaderMapCompileResults
{
	FShaderMapCompileResults() :
		bAllJobsSucceeded(true),
		bSkipResultProcessing(false),
		TimeStarted(FPlatformTime::Seconds()),
		bIsHung(false)
	{}

	void CheckIfHung();

	TArray<TRefCountPtr<class FShaderCommonCompileJob>> FinishedJobs;
	FThreadSafeCounter NumPendingJobs;
	bool bAllJobsSucceeded;
	bool bSkipResultProcessing;
	double TimeStarted;
	bool bIsHung;
};

struct FPendingShaderMapCompileResults
	: public FShaderMapCompileResults
	, public FRefCountBase
{};
using FPendingShaderMapCompileResultsPtr = TRefCountPtr<FPendingShaderMapCompileResults>;


/**
 * Cached reference to the location of an in-flight job's FShaderJobData in the FShaderJobDataMap, used by the private FShaderJobCache class.
 *
 * Caching the reference avoids the need to do additional map lookups to find the entry again, potentially avoiding a lock of the container for
 * the lookup.  Heap allocation of blocks is used by the cache to allow map entries to have a persistent location in memory.  The persistent
 * memory allows modifications of map entry data for a given job, without needing locks to protect against container resizing.
 *
 * In-flight jobs and their duplicates reference the same FShaderJobData.  Client code should treat this structure as opaque.
 */
struct FShaderJobCacheRef
{
	/** Pointer to block the private FShaderJobData is stored in */
	struct FShaderJobDataBlock* Block = nullptr;

	/** Index of FShaderJobData in the block */
	int32 IndexInBlock = INDEX_NONE;

	/** If job is a duplicate, index of pointer to job in DuplicateJobs array in FShaderJobCache, used for clearing the pointer when the in-flight job completes */
	int32 DuplicateIndex = INDEX_NONE;

	void Clear()
	{
		Block = nullptr;
		IndexInBlock = INDEX_NONE;
		DuplicateIndex = INDEX_NONE;
	}
};

/** Stores all of the common information used to compile a shader or pipeline. */
class FShaderCommonCompileJob
{
public:
	/** Linked list support -- not using TIntrusiveLinkedList because we want lock free insertion not supported by the core class */
	FShaderCommonCompileJob* NextLink = nullptr;
	FShaderCommonCompileJob** PrevLink = nullptr;

	using FInputHash = FBlake3Hash;

	FPendingShaderMapCompileResultsPtr PendingShaderMap;

	mutable FThreadSafeCounter NumRefs;
	int32 JobIndex;
	uint32 Hash;

	/** Id of the shader map this shader belongs to. */
	uint32 Id;

	EShaderCompileJobType Type;
	EShaderCompileJobPriority Priority;
	EShaderCompileJobPriority PendingPriority;
	EShaderCompilerWorkerType CurrentWorker;

	TPimplPtr<UE::DerivedData::FRequestOwner> RequestOwner;

	/** true if the results of the shader compile have been processed. */
	uint8 bFinalized : 1;
	/** Output of the shader compile */
	uint8 bSucceeded : 1;
	uint8 bErrorsAreLikelyToBeCode : 1;
	/** true if the results of the shader compile have been released from the FShaderCompilerManager.
		After a job is bFinalized it will be bReleased when ReleaseJob() is invoked, which means that the shader compile thread
		is no longer processing the job; which is useful for non standard job handling (Niagara as an example). */
	uint8 bReleased : 1;
	/** Whether we hashed the inputs */
	uint8 bInputHashSet : 1;
	/** Whether or not we are a default material. */
	uint8 bIsDefaultMaterial : 1;
	/** Whether or not we are a global shader. */
	uint8 bIsGlobalShader : 1;
	/** Hash of all the job inputs */
	FInputHash InputHash;

	/** In-engine timestamp of being added to a pending queue. Not set for jobs that are satisfied from the jobs cache */
	double TimeAddedToPendingQueue = 0.0;
	/** In-engine timestamp of being assigned to a worker. Not set for jobs that are satisfied from the jobs cache */
	double TimeAssignedToExecution = 0.0;
	/** In-engine timestamp of job being completed. Encompasses the compile time. Not set for jobs that are satisfied from the jobs cache */
	double TimeExecutionCompleted = 0.0;
	/** Time spent in tasks generated in FShaderJobCache::SubmitJobs, plus stall time on mutex locks in those tasks */
	double TimeTaskSubmitJobs = 0.0;
	double TimeTaskSubmitJobsStall = 0.0;

	FShaderJobCacheRef JobCacheRef;

	uint32 AddRef() const
	{
		return uint32(NumRefs.Increment());
	}

	uint32 Release() const
	{
		uint32 Refs = uint32(NumRefs.Decrement());
		if (Refs == 0)
		{
			Destroy();
		}
		return Refs;
	}
	uint32 GetRefCount() const
	{
		return uint32(NumRefs.GetValue());
	}

	/** Returns hash of all inputs for this job (needed for caching). */
	virtual FInputHash GetInputHash() { return FInputHash(); }

	/** Serializes (and deserializes) the output for caching purposes. */
	virtual void SerializeOutput(FArchive& Ar) {}

	FShaderCompileJob* GetSingleShaderJob();
	const FShaderCompileJob* GetSingleShaderJob() const;
	FShaderPipelineCompileJob* GetShaderPipelineJob();
	const FShaderPipelineCompileJob* GetShaderPipelineJob() const;

	// Executed for all jobs (including those read from cache) on completion.
	virtual void OnComplete() = 0;

	virtual void AppendDebugName(FStringBuilderBase& OutName) const = 0;
	
	bool Equals(const FShaderCommonCompileJob& Rhs) const;

	/** Calls the specified predicate for each single compile job, i.e. FShaderCompileJob and each stage of FShaderPipelineCompileJob. */
	void ForEachSingleShaderJob(const TFunction<void(const FShaderCompileJob& SingleJob)>& Predicate) const;

	/** This returns a unique id for a shader compiler job */
	RENDERCORE_API static uint32 GetNextJobId();

protected:
	friend class FShaderCompilingManager;
	friend class FShaderPipelineCompileJob;

	FShaderCommonCompileJob(EShaderCompileJobType InType, uint32 InHash, uint32 InId, EShaderCompileJobPriority InPriority) :
		NumRefs(0),
		JobIndex(INDEX_NONE),
		Hash(InHash),
		Id(InId),
		Type(InType),
		Priority(InPriority),
		PendingPriority(EShaderCompileJobPriority::None),
		CurrentWorker(EShaderCompilerWorkerType::None),
		bFinalized(false),
		bSucceeded(false),
		bErrorsAreLikelyToBeCode(false),
		bReleased(false),
		bInputHashSet(false),
		bIsDefaultMaterial(false),
		bIsGlobalShader(false)

	{
		check(InPriority != EShaderCompileJobPriority::None);
	}

	virtual ~FShaderCommonCompileJob() {}

private:
	/** Value counter for job ids. */
	static FThreadSafeCounter JobIdCounter;

	void Destroy() const;
};
using FShaderCommonCompileJobPtr = TRefCountPtr<FShaderCommonCompileJob>;

struct FShaderCompileJobKey
{
	explicit FShaderCompileJobKey(const FShaderType* InType = nullptr, const FVertexFactoryType* InVFType = nullptr, int32 InPermutationId = 0)
		: ShaderType(InType), VFType(InVFType), PermutationId(InPermutationId)
	{}

	uint32 MakeHash(uint32 Id) const { return HashCombine(HashCombine(HashCombine(GetTypeHash(Id), GetTypeHash(VFType)), GetTypeHash(ShaderType)), GetTypeHash(PermutationId)); }
	RENDERCORE_API FString ToString() const;
	const FShaderType* ShaderType;
	const FVertexFactoryType* VFType;
	int32 PermutationId;

	friend inline bool operator==(const FShaderCompileJobKey& Lhs, const FShaderCompileJobKey& Rhs)
	{
		return Lhs.VFType == Rhs.VFType && Lhs.ShaderType == Rhs.ShaderType && Lhs.PermutationId == Rhs.PermutationId;
	}
	friend inline bool operator!=(const FShaderCompileJobKey& Lhs, const FShaderCompileJobKey& Rhs)
	{
		return !operator==(Lhs, Rhs);
	}
};

/** Stores all of the input and output information used to compile a single shader. */
class FShaderCompileJob : public FShaderCommonCompileJob
{
public:
	static const EShaderCompileJobType Type = EShaderCompileJobType::Single;

	FShaderCompileJobKey Key;

	/** 
	 * Additional parameters that can be supplied to the compile job such 
	 * that it is available from the compilation begins to when the FShader is created.
	 */
	TSharedPtr<const FShaderType::FParameters, ESPMode::ThreadSafe> ShaderParameters;

	/** Input for the shader compile */
	FShaderCompilerInput Input;
	FShaderPreprocessOutput PreprocessOutput;
	TUniquePtr<FShaderPreprocessOutput> SecondaryPreprocessOutput{};
	FShaderCompilerOutput Output;
	TUniquePtr<FShaderCompilerOutput> SecondaryOutput{};

	// List of pipelines that are sharing this job.
	TMap<const FVertexFactoryType*, TArray<const FShaderPipelineType*>> SharingPipelines;

	virtual RENDERCORE_API FInputHash GetInputHash() override;
	virtual RENDERCORE_API void SerializeOutput(FArchive& Ar) override;

	virtual RENDERCORE_API void OnComplete() override;
	
	virtual RENDERCORE_API void AppendDebugName(FStringBuilderBase& OutName) const override;

	// Serializes only the subset of data written by SCW/read back from ShaderCompiler when using worker processes.
	RENDERCORE_API void SerializeWorkerOutput(FArchive& Ar);
	
	// Serializes only the subset of data written by ShaderCompiler and read from SCW when using worker processes.
	RENDERCORE_API void SerializeWorkerInput(FArchive& Ar);

	UE_DEPRECATED(5.4, "GetFinalSource is deprecated, GetFinalSourceView returns an FStringView instead.")
	inline const FString& GetFinalSource() const
	{
		static FString Empty;
		return Empty;
	}

	RENDERCORE_API FStringView GetFinalSourceView() const;

	FShaderCompileJob() : FShaderCommonCompileJob(Type, 0u, 0u, EShaderCompileJobPriority::Num)
	{}

	FShaderCompileJob(uint32 InHash, uint32 InId, EShaderCompileJobPriority InPriroity, const FShaderCompileJobKey& InKey) :
		FShaderCommonCompileJob(Type, InHash, InId, InPriroity),
		Key(InKey)
	{}
};

struct FShaderPipelineCompileJobKey
{
	explicit FShaderPipelineCompileJobKey(const FShaderPipelineType* InType = nullptr, const FVertexFactoryType* InVFType = nullptr, int32 InPermutationId = 0)
		: ShaderPipeline(InType), VFType(InVFType), PermutationId(InPermutationId)
	{}

	uint32 MakeHash(uint32 Id) const { return HashCombine(HashCombine(HashCombine(GetTypeHash(Id), GetTypeHash(ShaderPipeline)), GetTypeHash(VFType)), GetTypeHash(PermutationId)); }

	const FShaderPipelineType* ShaderPipeline;
	const FVertexFactoryType* VFType;
	int32 PermutationId;

	friend inline bool operator==(const FShaderPipelineCompileJobKey& Lhs, const FShaderPipelineCompileJobKey& Rhs)
	{
		return Lhs.ShaderPipeline == Rhs.ShaderPipeline && Lhs.VFType == Rhs.VFType && Lhs.PermutationId == Rhs.PermutationId;
	}
	friend inline bool operator!=(const FShaderPipelineCompileJobKey& Lhs, const FShaderPipelineCompileJobKey& Rhs)
	{
		return !operator==(Lhs, Rhs);
	}
};

class FShaderPipelineCompileJob : public FShaderCommonCompileJob
{
public:
	static const EShaderCompileJobType Type = EShaderCompileJobType::Pipeline;

	FShaderPipelineCompileJobKey Key;
	TArray<TRefCountPtr<FShaderCompileJob>> StageJobs;
	UE_DEPRECATED(5.3, "bFailedRemovingUnused field is no longer used")
	bool bFailedRemovingUnused;

	virtual RENDERCORE_API FInputHash GetInputHash() override;
	virtual RENDERCORE_API void SerializeOutput(FArchive& Ar) override;
	virtual RENDERCORE_API void OnComplete() override;
	virtual RENDERCORE_API void AppendDebugName(FStringBuilderBase& OutName) const override;

	RENDERCORE_API FShaderPipelineCompileJob(int32 NumStages);
	RENDERCORE_API FShaderPipelineCompileJob(uint32 InHash, uint32 InId, EShaderCompileJobPriority InPriroity, const FShaderPipelineCompileJobKey& InKey);
};

inline FShaderCompileJob* FShaderCommonCompileJob::GetSingleShaderJob() { return Type == EShaderCompileJobType::Single ? static_cast<FShaderCompileJob*>(this) : nullptr; }
inline const FShaderCompileJob* FShaderCommonCompileJob::GetSingleShaderJob() const { return Type == EShaderCompileJobType::Single ? static_cast<const FShaderCompileJob*>(this) : nullptr; }
inline FShaderPipelineCompileJob* FShaderCommonCompileJob::GetShaderPipelineJob() { return Type == EShaderCompileJobType::Pipeline ? static_cast<FShaderPipelineCompileJob*>(this) : nullptr; }
inline const FShaderPipelineCompileJob* FShaderCommonCompileJob::GetShaderPipelineJob() const { return Type == EShaderCompileJobType::Pipeline ? static_cast<const FShaderPipelineCompileJob*>(this) : nullptr; }

inline bool FShaderCommonCompileJob::Equals(const FShaderCommonCompileJob& Rhs) const
{
	if (Type == Rhs.Type && Id == Rhs.Id)
	{
		switch (Type)
		{
		case EShaderCompileJobType::Single: return static_cast<const FShaderCompileJob*>(this)->Key == static_cast<const FShaderCompileJob&>(Rhs).Key;
		case EShaderCompileJobType::Pipeline: return static_cast<const FShaderPipelineCompileJob*>(this)->Key == static_cast<const FShaderPipelineCompileJob&>(Rhs).Key;
		default: checkNoEntry(); break;
		}
	}
	return false;
}

inline void FShaderCommonCompileJob::Destroy() const
{
	switch (Type)
	{
	case EShaderCompileJobType::Single: delete static_cast<const FShaderCompileJob*>(this); break;
	case EShaderCompileJobType::Pipeline: delete static_cast<const FShaderPipelineCompileJob*>(this); break;
	default: checkNoEntry();
	}
}

inline void FShaderCommonCompileJob::ForEachSingleShaderJob(const TFunction<void(const FShaderCompileJob&)>& Predicate) const
{
	if (const FShaderCompileJob* SingleJob = GetSingleShaderJob())
	{
		Predicate(*SingleJob);
	}
	else if (const FShaderPipelineCompileJob* PipelineJob = GetShaderPipelineJob())
	{
		for (const TRefCountPtr<FShaderCompileJob>& StageJob : PipelineJob->StageJobs)
		{
			if (const FShaderCompileJob* SingleStageJob = StageJob->GetSingleShaderJob())
			{
				Predicate(*SingleStageJob);
			}
		}
	}
}
