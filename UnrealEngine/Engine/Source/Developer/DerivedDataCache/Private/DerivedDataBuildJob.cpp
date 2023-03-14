// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildJob.h"

#include "Algo/Find.h"
#include "Compression/CompressedBuffer.h"
#include "DerivedDataBuild.h"
#include "DerivedDataBuildAction.h"
#include "DerivedDataBuildDefinition.h"
#include "DerivedDataBuildFunction.h"
#include "DerivedDataBuildFunctionRegistry.h"
#include "DerivedDataBuildInputResolver.h"
#include "DerivedDataBuildInputs.h"
#include "DerivedDataBuildJobContext.h"
#include "DerivedDataBuildKey.h"
#include "DerivedDataBuildOutput.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataBuildScheduler.h"
#include "DerivedDataBuildTypes.h"
#include "DerivedDataBuildWorker.h"
#include "DerivedDataBuildWorkerRegistry.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataSharedString.h"
#include "HAL/CriticalSection.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "String/ParseTokens.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"

namespace UE::DerivedData::Private
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** A build job proceeds through these states in order and may skip states. */
enum class EBuildJobState : uint32
{
	/** A new job that has not been scheduled. */
	NotStarted,
	/** Resolve the key to a definition. */
	ResolveKey,
	/** Wait for the input resolver to resolve the key. */
	ResolveKeyWait,
	/** Resolve each input to its raw hash and raw size. */
	ResolveInputMeta,
	/** Wait for the input resolver to resolve the metadata. */
	ResolveInputMetaWait,
	/** Query the cache for the action. */
	CacheQuery,
	/** Wait for the cache query to finish. */
	CacheQueryWait,
	/** Try to execute remotely without loading additional inputs. */
	ExecuteRemote,
	/** Wait for the remote execution request to finish. */
	ExecuteRemoteWait,
	/** Resolve each input that was missing from remote execution. */
	ResolveRemoteInputData,
	/** Wait for the input resolver to resolve the data. */
	ResolveRemoteInputDataWait,
	/** Try to execute remotely after resolving missing inputs. */
	ExecuteRemoteRetry,
	/** Wait for the remote execution request to finish. */
	ExecuteRemoteRetryWait,
	/** Resolve each input that is not yet resolved. */
	ResolveInputData,
	/** Wait for the input resolver to resolve the data. */
	ResolveInputDataWait,
	/** Execute the function locally to build the output. */
	ExecuteLocal,
	/** Wait for the async function to finish executing locally. */
	ExecuteLocalWait,
	/** Store the output in the cache. */
	CacheStore,
	/** Wait for the cache store to finish. */
	CacheStoreWait,
	/** Complete. */
	Complete,
};

class FBuildJob final : public IBuildJob
{
public:
	/** Resolve the key to a definition, then build like the definition constructor. */
	FBuildJob(
		const FBuildJobCreateParams& Params,
		const FBuildKey& Key,
		const FBuildPolicy& Policy,
		FOnBuildComplete&& OnComplete);

	/** Resolve the definition to an action, then build like the action constructor. */
	FBuildJob(
		const FBuildJobCreateParams& Params,
		const FBuildDefinition& Definition,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		FOnBuildComplete&& OnComplete);

	/** Query the cache, attempt remote execution, resolve inputs, fall back to local execution, store to the cache. */
	FBuildJob(
		const FBuildJobCreateParams& Params,
		const FBuildAction& Action,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		FOnBuildComplete&& OnComplete);

	/** Destroy the job, which must be complete or not started. */
	~FBuildJob();

	// IBuildJob Interface

	inline const FSharedString& GetName() const final { return Name; }
	inline const FUtf8SharedString& GetFunction() const final { return FunctionName; }

	inline ICache& GetCache() const final { return Cache; }
	inline IBuild& GetBuild() const final { return BuildSystem; }

	void StepExecution() final;

private:
	FBuildJob(
		const FBuildJobCreateParams& Params,
		const FSharedString& Name,
		const FUtf8SharedString& FunctionName,
		FOnBuildComplete&& OnComplete);

	void BeginJob();
	void EndJob();

	void CreateContext();

	void EnterCacheQuery();
	void EnterCacheStore();
	void EnterResolveKey();
	void EnterResolveInputMeta();
	void EnterResolveInputData();
	void EnterExecuteRemote();
	void EnterExecuteLocal();

	void BeginCacheQuery();
	void BeginCacheStore();
	void BeginResolveKey();
	void BeginResolveInputMeta();
	void BeginResolveInputData();
	void BeginExecuteRemote();
	void BeginExecuteLocal();

	void EndCacheQuery(FCacheGetResponse&& Response);
	void EndCacheStore(FCachePutResponse&& Response);
	void EndResolveKey(FBuildKeyResolvedParams&& Params);
	void EndResolveInputMeta(FBuildInputMetaResolvedParams&& Params);
	void EndResolveInputData(FBuildInputDataResolvedParams&& Params);
	void EndExecuteRemote(FBuildWorkerActionCompleteParams&& Params);
	void EndExecuteLocal();

	void SkipExecuteRemote() final;

	void CreateAction(TConstArrayView<FBuildInputMetaByKey> InputMeta);

	void SetDefinition(FBuildDefinition&& Definition);
	void SetAction(FBuildAction&& Action);
	void SetInputs(FBuildInputs&& Inputs);
	void SetOutput(const FBuildOutput& Output) final;
	void SetOutputNoCheck(FBuildOutput&& Output, EBuildJobState NewState = EBuildJobState::CacheStore);

	/** Terminate the job and send the error to the output complete callback. */
	void CompleteWithError(FUtf8StringView Error);

	/** Advance to the new state, dispatching to the scheduler and invoking callbacks as appropriate. */
	void AdvanceToState(EBuildJobState NewState);

	/** Execute a transition from the old state to the new state. */
	void ExecuteTransition(EBuildJobState OldState, EBuildJobState NewState);

	/** Execute the new state. */
	void ExecuteState(EBuildJobState NewState);

	/** Exports the action and inputs for this build to disk. */
	void ExportBuild() const;

	/** Determine whether the action and inputs for this build should be exported to disk. */
	bool ShouldExportBuild() const;

	/** Parse the types to export from the command line. Sorted by FNameFastLess. */
	static TArray<FName> ParseExportBuildTypes(bool& bOutExportAll);

private:
	FSharedString Name;
	FUtf8SharedString FunctionName{UTF8TEXT("Unknown")};

	/** Active state for the job. Always moves through states in order. */
	EBuildJobState State{EBuildJobState::NotStarted};
	/** Next state for the job. Used to handle re-entrant calls to AdvanceToState. */
	EBuildJobState NextState{EBuildJobState::NotStarted};

	/** Policy requested and/or configured for the job. */
	FBuildPolicy BuildPolicy{EBuildPolicy::None};
	/** Status flags that are added to as the job moves through its states. */
	EBuildStatus BuildStatus{EBuildStatus::None};

	/** Available in [ResolveKey, Complete] for jobs created from a key or definition. */
	FBuildKey DefinitionKey;
	/** Available in [ResolveAction, ExecuteLocal) for jobs created from a key or definition. */
	FOptionalBuildDefinition Definition;
	/** Available in [CacheQuery, CacheStore). */
	FOptionalBuildAction Action;
	/** Available in [ExecuteLocal, CacheStore) for jobs with non-constant inputs. */
	FOptionalBuildInputs Inputs;
	/** Available in [CacheStore, Complete). */
	FOptionalBuildOutput Output;

	/** Available in [NotStarted, CacheStore). */
	FBuildOutputBuilder OutputBuilder;

	/** Available in [CacheQuery, CacheStoreWait). Context used by IBuildFunction. */
	TRefCountPtr<FBuildJobContext> Context;

	/** Available in (NotStarted, Complete). */
	TUniquePtr<IBuildJobSchedule> Schedule;

	/** Lock to synchronize writes to State, NextState, bInAdvanceToState. */
	FRWLock Lock;
	/** Owner used to synchronize every async operation executed by the job. */
	IRequestOwner& Owner;
	/** Scheduler that the job is scheduled on. Null before Schedule() and after completion. */
	IBuildScheduler& Scheduler;
	/** Resolver for definitions and inputs. May be null for a job with nothing to resolve. */
	IBuildInputResolver* InputResolver{};
	/** Worker to use for remote execution. Available in [ExecuteRemote, ExecuteRemoteRetryWait]. */
	FBuildWorker* Worker{};
	/** Worker executor to use for remote execution. Available in [ExecuteRemote, ExecuteRemoteRetryWait]. */
	IBuildWorkerExecutor* WorkerExecutor{};
	/** Invoked exactly once when the output is complete or when the job fails. */
	FOnBuildComplete OnComplete;

	/** Keys for missing inputs. */
	TArray<FUtf8StringView> MissingInputs;

	ICache& Cache;
	IBuild& BuildSystem;

	/** True if AdvanceToState is executing. */
	bool bInAdvanceToState{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* LexToString(EBuildJobState State)
{
	switch (State)
	{
	case EBuildJobState::NotStarted:                 return TEXT("NotStarted");
	case EBuildJobState::ResolveKey:                 return TEXT("ResolveKey");
	case EBuildJobState::ResolveKeyWait:             return TEXT("ResolveKeyWait");
	case EBuildJobState::ResolveInputMeta:           return TEXT("ResolveInputMeta");
	case EBuildJobState::ResolveInputMetaWait:       return TEXT("ResolveInputMetaWait");
	case EBuildJobState::CacheQuery:                 return TEXT("CacheQuery");
	case EBuildJobState::CacheQueryWait:             return TEXT("CacheQueryWait");
	case EBuildJobState::ExecuteRemote:              return TEXT("ExecuteRemote");
	case EBuildJobState::ExecuteRemoteWait:          return TEXT("ExecuteRemoteWait");
	case EBuildJobState::ResolveRemoteInputData:     return TEXT("ResolveRemoteInputData");
	case EBuildJobState::ResolveRemoteInputDataWait: return TEXT("ResolveRemoteInputDataWait");
	case EBuildJobState::ExecuteRemoteRetry:         return TEXT("ExecuteRemoteRetry");
	case EBuildJobState::ExecuteRemoteRetryWait:     return TEXT("ExecuteRemoteRetryWait");
	case EBuildJobState::ResolveInputData:           return TEXT("ResolveInputData");
	case EBuildJobState::ResolveInputDataWait:       return TEXT("ResolveInputDataWait");
	case EBuildJobState::ExecuteLocal:               return TEXT("ExecuteLocal");
	case EBuildJobState::ExecuteLocalWait:           return TEXT("ExecuteLocalWait");
	case EBuildJobState::CacheStore:                 return TEXT("CacheStore");
	case EBuildJobState::CacheStoreWait:             return TEXT("CacheStoreWait");
	case EBuildJobState::Complete:                   return TEXT("Complete");
	default: checkNoEntry();                         return TEXT("Unknown");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildJob::FBuildJob(
	const FBuildJobCreateParams& Params,
	const FSharedString& InName,
	const FUtf8SharedString& InFunctionName,
	FOnBuildComplete&& InOnComplete)
	: Name(InName)
	, FunctionName(InFunctionName)
	, OutputBuilder(Params.BuildSystem.CreateOutput(Name, FunctionName))
	, Owner(Params.Owner)
	, Scheduler(Params.Scheduler)
	, InputResolver(Params.InputResolver)
	, OnComplete(MoveTemp(InOnComplete))
	, Cache(Params.Cache)
	, BuildSystem(Params.BuildSystem)
{
}

FBuildJob::FBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildKey& InKey,
	const FBuildPolicy& InPolicy,
	FOnBuildComplete&& InOnComplete)
	: FBuildJob(Params, {WriteToString<64>(TEXTVIEW("Resolve: "), InKey)}, UTF8TEXTVIEW("Unknown"), MoveTemp(InOnComplete))
{
	BuildPolicy = InPolicy;
	DefinitionKey = InKey;
	FRequestBarrier Barrier(Owner, ERequestBarrierFlags::Priority);
	AdvanceToState(EBuildJobState::ResolveKey);
}

FBuildJob::FBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildDefinition& InDefinition,
	const FOptionalBuildInputs& InInputs,
	const FBuildPolicy& InPolicy,
	FOnBuildComplete&& InOnComplete)
	: FBuildJob(Params, InDefinition.GetName(), InDefinition.GetFunction(), MoveTemp(InOnComplete))
{
	BuildPolicy = InPolicy;
	DefinitionKey = InDefinition.GetKey();
	Definition = InDefinition;
	Inputs = InInputs;
	FRequestBarrier Barrier(Owner, ERequestBarrierFlags::Priority);
	AdvanceToState(EBuildJobState::ResolveKey);
}

FBuildJob::FBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildAction& InAction,
	const FOptionalBuildInputs& InInputs,
	const FBuildPolicy& InPolicy,
	FOnBuildComplete&& InOnComplete)
	: FBuildJob(Params, InAction.GetName(), InAction.GetFunction(), MoveTemp(InOnComplete))
{
	BuildPolicy = InPolicy;
	Action = InAction;
	Inputs = InInputs;
	FRequestBarrier Barrier(Owner, ERequestBarrierFlags::Priority);
	AdvanceToState(EBuildJobState::ResolveKey);
}

FBuildJob::~FBuildJob()
{
	checkf(State == EBuildJobState::NotStarted || State == EBuildJobState::Complete,
		TEXT("Job in state %s must complete before being destroyed for build of '%s' by %s."),
		LexToString(State), *Name, *WriteToString<32>(FunctionName));
	checkf(!OnComplete,
		TEXT("Job in state %s must invoke its completion callback before being destroyed for build of '%s' by %s."),
		LexToString(State), *Name, *WriteToString<32>(FunctionName));
}

void FBuildJob::StepExecution()
{
	switch (State)
	{
	case EBuildJobState::ResolveKey:             return AdvanceToState(EBuildJobState::ResolveKeyWait);
	case EBuildJobState::ResolveInputMeta:       return AdvanceToState(EBuildJobState::ResolveInputMetaWait);
	case EBuildJobState::CacheQuery:             return AdvanceToState(EBuildJobState::CacheQueryWait);
	case EBuildJobState::ExecuteRemote:          return AdvanceToState(EBuildJobState::ExecuteRemoteWait);
	case EBuildJobState::ResolveRemoteInputData: return AdvanceToState(EBuildJobState::ResolveRemoteInputDataWait);
	case EBuildJobState::ExecuteRemoteRetry:     return AdvanceToState(EBuildJobState::ExecuteRemoteRetryWait);
	case EBuildJobState::ResolveInputData:       return AdvanceToState(EBuildJobState::ResolveInputDataWait);
	case EBuildJobState::ExecuteLocal:           return AdvanceToState(EBuildJobState::ExecuteLocalWait);
	case EBuildJobState::CacheStore:             return AdvanceToState(EBuildJobState::CacheStoreWait);
	default:
		checkf(false,
			TEXT("Job in state %s is not valid to be scheduled for build of '%s' by %s."),
			LexToString(State), *Name, *WriteToString<32>(FunctionName));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::BeginJob()
{
	check(!Schedule);
	Schedule = Scheduler.BeginJob(*this, Owner);
}

void FBuildJob::EndJob()
{
	Schedule->EndJob();
	delete this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::CreateContext()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBuildJob::CreateContext);

	const IBuildFunction* Function = BuildSystem.GetFunctionRegistry().FindFunction(FunctionName);
	if (BuildSystem.GetVersion() != Action.Get().GetBuildSystemVersion())
	{
		return CompleteWithError(WriteToUtf8String<192>(ANSITEXTVIEW("Failed because the build system is version "),
			BuildSystem.GetVersion(), ANSITEXTVIEW(" when version "),
			Action.Get().GetBuildSystemVersion(), ANSITEXTVIEW(" is expected.")));
	}
	else if (!Function)
	{
		return CompleteWithError(WriteToUtf8String<128>(ANSITEXTVIEW("Failed because the function "), FunctionName,
			ANSITEXTVIEW(" was not found.")));
	}
	else if (!Function->GetVersion().IsValid())
	{
		return CompleteWithError(WriteToUtf8String<128>(ANSITEXTVIEW("Failed because the function "), FunctionName,
			ANSITEXTVIEW(" has a version of zero.")));
	}
	else if (Function->GetVersion() != Action.Get().GetFunctionVersion())
	{
		return CompleteWithError(WriteToUtf8String<192>(ANSITEXTVIEW("Failed because the function "), FunctionName,
			ANSITEXTVIEW(" is version "), Function->GetVersion(), ANSITEXTVIEW(" when version "),
			Action.Get().GetFunctionVersion(), ANSITEXTVIEW(" is expected.")));
	}
	else
	{
		const FCacheKey CacheKey{FCacheBucket(FunctionName), Action.Get().GetKey().Hash};
		Context = new FBuildJobContext(*this, CacheKey, *Function, OutputBuilder);
		Action.Get().IterateConstants([this](FUtf8StringView Key, FCbObject&& Value)
		{
			Context->AddConstant(Key, MoveTemp(Value));
		});
		Function->Configure(*Context);
		EnumAddFlags(BuildStatus, ShouldExportBuild() ? EBuildStatus::BuildTryExport : EBuildStatus::None);
		EnumAddFlags(BuildStatus, EBuildStatus::CacheKey);
	}

	if (OutputBuilder.HasError())
	{
		return AdvanceToState(EBuildJobState::Complete);
	}

	// Populate the scheduler params with the information that is available now.
	FBuildSchedulerParams& SchedulerParams = Schedule->EditParameters();
	SchedulerParams.Key = Action.Get().GetKey();
	Action.Get().IterateConstants([&SchedulerParams](FUtf8StringView Key, FCbObject&& Value)
	{
		const uint64 ValueSize = Value.GetSize();
		SchedulerParams.TotalInputsSize += ValueSize;
		SchedulerParams.ResolvedInputsSize += ValueSize;
	});
	Action.Get().IterateInputs([&SchedulerParams](FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)
	{
		SchedulerParams.TotalInputsSize += RawSize;
	});
	if (Inputs)
	{
		Inputs.Get().IterateInputs([&SchedulerParams](FUtf8StringView Key, const FCompressedBuffer& Buffer)
		{
			SchedulerParams.ResolvedInputsSize += Buffer.GetRawSize();
		});
	}
	SchedulerParams.TotalRequiredMemory = SchedulerParams.TotalInputsSize + Context->GetRequiredMemory();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static ECachePolicy MakeCacheQueryPolicy(EBuildPolicy BuildPolicy, const FBuildJobContext& Context)
{
	BuildPolicy &= Context.GetBuildPolicyMask();
	ECachePolicy CachePolicy = ECachePolicy::None;
	if (EnumHasAnyFlags(BuildPolicy, EBuildPolicy::CacheQuery))
	{
		EnumAddFlags(CachePolicy, ECachePolicy::Query);
	}
	if (EnumHasAnyFlags(BuildPolicy, EBuildPolicy::CacheStoreOnQuery))
	{
		EnumAddFlags(CachePolicy, ECachePolicy::Store);
	}
	if (EnumHasAnyFlags(BuildPolicy, EBuildPolicy::CacheKeepAlive))
	{
		EnumAddFlags(CachePolicy, ECachePolicy::KeepAlive);
	}
	if (EnumHasAnyFlags(BuildPolicy, EBuildPolicy::SkipData))
	{
		EnumAddFlags(CachePolicy, ECachePolicy::SkipData);
	}
	CachePolicy &= Context.GetCachePolicyMask();
	return CachePolicy;
}

static FCacheRecordPolicy MakeCacheRecordQueryPolicy(const FBuildPolicy& BuildPolicy, const FBuildJobContext& Context)
{
	const ECachePolicy DefaultCachePolicy = MakeCacheQueryPolicy(BuildPolicy.GetDefaultPolicy(), Context);
	const ECachePolicy CombinedCachePolicy = MakeCacheQueryPolicy(BuildPolicy.GetCombinedPolicy(), Context);

	FCacheRecordPolicyBuilder Builder(DefaultCachePolicy | (CombinedCachePolicy & ECachePolicy::KeepAlive));
	for (const FBuildValuePolicy& Value : BuildPolicy.GetValuePolicies())
	{
		Builder.AddValuePolicy(Value.Id, MakeCacheQueryPolicy(Value.Policy, Context) & ~ECachePolicy::KeepAlive);
	}

	// Output can only be loaded if its value is present. Add a policy for the output value which
	// is compatible with the combined policy if that differs from the default policy.
	if (EnumHasAnyFlags(CombinedCachePolicy, ECachePolicy::Query) &&
		(EnumHasAnyFlags(DefaultCachePolicy, ECachePolicy::SkipData) ||
		(CombinedCachePolicy & ECachePolicy::Default) != (DefaultCachePolicy & ECachePolicy::Default)))
	{
		Builder.AddValuePolicy(GetBuildOutputValueId(), CombinedCachePolicy & ~ECachePolicy::SkipData);
	}

	return Builder.Build();
}

void FBuildJob::EnterCacheQuery()
{
	if (!Context ||
		!EnumHasAnyFlags(Context->GetCachePolicyMask(), ECachePolicy::Query) ||
		!EnumHasAnyFlags(Context->GetBuildPolicyMask() & BuildPolicy.GetCombinedPolicy(), EBuildPolicy::CacheQuery))
	{
		return AdvanceToState(EBuildJobState::ExecuteRemote);
	}
}

void FBuildJob::BeginCacheQuery()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBuildJob::CacheQuery);
	EnumAddFlags(BuildStatus, EBuildStatus::CacheQuery);
	Cache.Get({{Name, Context->GetCacheKey(), MakeCacheRecordQueryPolicy(BuildPolicy, *Context)}}, Owner,
		[this](FCacheGetResponse&& Response) { EndCacheQuery(MoveTemp(Response)); });
}

void FBuildJob::EndCacheQuery(FCacheGetResponse&& Response)
{
	if (Response.Status == EStatus::Canceled)
	{
		return CompleteWithError(ANSITEXTVIEW("Build was canceled."));
	}
	if (Response.Status == EStatus::Ok)
	{
		if (FOptionalBuildOutput CacheOutput = FBuildOutput::Load(Name, FunctionName, Response.Record))
		{
			EnumAddFlags(BuildStatus, EBuildStatus::CacheQueryHit);
			return SetOutputNoCheck(MoveTemp(CacheOutput).Get());
		}
	}
	if (!EnumHasAnyFlags(BuildPolicy.GetCombinedPolicy() & Context->GetBuildPolicyMask(), EBuildPolicy::Build))
	{
		return CompleteWithError(ANSITEXTVIEW("Failed to fetch from the cache and build policy does not allow execution."));
	}
	return AdvanceToState(EBuildJobState::ExecuteRemote);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static ECachePolicy MakeCacheStorePolicy(EBuildPolicy BuildPolicy, const FBuildJobContext& Context)
{
	BuildPolicy &= Context.GetBuildPolicyMask();
	ECachePolicy CachePolicy = ECachePolicy::None;
	if (EnumHasAnyFlags(BuildPolicy, EBuildPolicy::CacheQuery))
	{
		EnumAddFlags(CachePolicy, ECachePolicy::Query);
	}
	if (EnumHasAnyFlags(BuildPolicy, EBuildPolicy::CacheStoreOnBuild))
	{
		EnumAddFlags(CachePolicy, ECachePolicy::Store);
	}
	if (EnumHasAnyFlags(BuildPolicy, EBuildPolicy::CacheKeepAlive))
	{
		EnumAddFlags(CachePolicy, ECachePolicy::KeepAlive);
	}
	CachePolicy &= Context.GetCachePolicyMask();
	return CachePolicy;
}

void FBuildJob::EnterCacheStore()
{
	if (!Context ||
		!EnumHasAnyFlags(Context->GetCachePolicyMask(), ECachePolicy::Store) ||
		!EnumHasAnyFlags(Context->GetBuildPolicyMask() & BuildPolicy.GetCombinedPolicy(), EBuildPolicy::CacheStoreOnBuild) ||
		EnumHasAnyFlags(BuildStatus, EBuildStatus::CacheQueryHit) ||
		Output.Get().HasError() ||
		Output.Get().HasLogs())
	{
		return AdvanceToState(EBuildJobState::Complete);
	}
}

void FBuildJob::BeginCacheStore()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBuildJob::CacheStore);
	EnumAddFlags(BuildStatus, EBuildStatus::CacheStore);
	FCacheRecordBuilder RecordBuilder(Context->GetCacheKey());
	Output.Get().Save(RecordBuilder);
	Cache.Put({{Name, RecordBuilder.Build(), MakeCacheStorePolicy(BuildPolicy.GetCombinedPolicy(), *Context)}}, Owner,
		[this](FCachePutResponse&& Response) { EndCacheStore(MoveTemp(Response)); });
}

void FBuildJob::EndCacheStore(FCachePutResponse&& Response)
{
	if (Response.Status == EStatus::Ok)
	{
		EnumAddFlags(BuildStatus, EBuildStatus::CacheStoreHit);
	}
	return AdvanceToState(EBuildJobState::Complete);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::EnterResolveKey()
{
	if (Action)
	{
		return AdvanceToState(EBuildJobState::CacheQuery);
	}
	if (Definition)
	{
		return AdvanceToState(EBuildJobState::ResolveInputMeta);
	}
	if (DefinitionKey != FBuildKey::Empty)
	{
		return CompleteWithError(ANSITEXTVIEW("Failed to resolve null key."));
	}
	if (!InputResolver)
	{
		return CompleteWithError(ANSITEXTVIEW("Failed to resolve key due to null input resolver."));
	}
}

void FBuildJob::BeginResolveKey()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBuildJob::ResolveKey);
	InputResolver->ResolveKey(DefinitionKey, Owner,
		[this](FBuildKeyResolvedParams&& Params) { EndResolveKey(MoveTemp(Params)); });
}

void FBuildJob::EndResolveKey(FBuildKeyResolvedParams&& Params)
{
	if (Params.Status == EStatus::Canceled)
	{
		return CompleteWithError(ANSITEXTVIEW("Build was canceled."));
	}
	if (Params.Status == EStatus::Ok && Params.Definition)
	{
		return SetDefinition(MoveTemp(Params.Definition).Get());
	}
	else
	{
		return CompleteWithError(WriteToUtf8String<128>(ANSITEXTVIEW("Failed to resolve key "), Params.Key, ANSITEXTVIEW(".")));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::EnterResolveInputMeta()
{
	if (!Definition.Get().HasInputs())
	{
		return CreateAction({});
	}
	if (!InputResolver)
	{
		return CompleteWithError(ANSITEXTVIEW("Failed to resolve input metadata due to null input resolver."));
	}
}

void FBuildJob::BeginResolveInputMeta()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBuildJob::ResolveInputMeta);
	InputResolver->ResolveInputMeta(Definition.Get(), Owner,
		[this](FBuildInputMetaResolvedParams&& Params) { EndResolveInputMeta(MoveTemp(Params)); });
}

void FBuildJob::EndResolveInputMeta(FBuildInputMetaResolvedParams&& Params)
{
	if (Params.Status == EStatus::Canceled)
	{
		return CompleteWithError(ANSITEXTVIEW("Build was canceled."));
	}
	if (Params.Status == EStatus::Ok)
	{
		return CreateAction(Params.Inputs);
	}
	else
	{
		return CompleteWithError(ANSITEXTVIEW("Failed to resolve input metadata."));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::EnterResolveInputData()
{
	FBuildSchedulerParams& SchedulerParams = Schedule->EditParameters();
	SchedulerParams.MissingLocalInputsSize = 0;
	SchedulerParams.MissingRemoteInputsSize = 0;
	if (State == EBuildJobState::ResolveInputData)
	{
		MissingInputs.Reset();
		Action.Get().IterateInputs([this, &SchedulerParams](FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)
		{
			if (!Inputs || Inputs.Get().FindInput(Key).IsNull())
			{
				MissingInputs.Emplace(Key);
				SchedulerParams.MissingLocalInputsSize += RawSize;
			}
		});
		if (MissingInputs.IsEmpty())
		{
			return AdvanceToState(EBuildJobState::ExecuteLocal);
		}
		// Skip resolving input data if it will not be used anyway.
		if (!EnumHasAnyFlags(BuildPolicy.GetCombinedPolicy() & Context->GetBuildPolicyMask(), EBuildPolicy::BuildLocal) &&
			!EnumHasAnyFlags(BuildStatus, EBuildStatus::BuildTryExport))
		{
			return AdvanceToState(EBuildJobState::ExecuteLocal);
		}
	}
	else
	{
		Action.Get().IterateInputs([this, &SchedulerParams](FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)
		{
			if (MissingInputs.FindByKey(Key))
			{
				SchedulerParams.MissingRemoteInputsSize += RawSize;
			}
		});
	}

	checkf(!MissingInputs.IsEmpty(),
		TEXT("Job is not expected to be in state %s without missing inputs for build of '%s' by %s."),
		LexToString(State), *Name, *WriteToString<32>(FunctionName));

	if (!InputResolver)
	{
		return CompleteWithError(ANSITEXTVIEW("Failed to resolve input data due to null input resolver."));
	}
}

void FBuildJob::BeginResolveInputData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBuildJob::ResolveInputData);
	if (Definition)
	{
		InputResolver->ResolveInputData(Definition.Get(), Owner,
			[this](FBuildInputDataResolvedParams&& Params) { EndResolveInputData(MoveTemp(Params)); },
			[this](FUtf8StringView Key) { return !!Algo::Find(MissingInputs, Key); });
	}
	else
	{
		InputResolver->ResolveInputData(Action.Get(), Owner,
			[this](FBuildInputDataResolvedParams&& Params) { EndResolveInputData(MoveTemp(Params)); },
			[this](FUtf8StringView Key) { return !!Algo::Find(MissingInputs, Key); });
	}
}

void FBuildJob::EndResolveInputData(FBuildInputDataResolvedParams&& Params)
{
	if (Params.Status == EStatus::Canceled)
	{
		return CompleteWithError(ANSITEXTVIEW("Build was canceled."));
	}
	if (Params.Status == EStatus::Ok)
	{
		FBuildSchedulerParams& SchedulerParams = Schedule->EditParameters();
		FBuildInputsBuilder Builder = BuildSystem.CreateInputs(Name);
		for (const FBuildInputDataByKey& Input : Params.Inputs)
		{
			Builder.AddInput(Input.Key, Input.Data);
			SchedulerParams.ResolvedInputsSize += Input.Data.GetRawSize();
		}
		if (Inputs)
		{
			Inputs.Get().IterateInputs([&Builder](FUtf8StringView Key, const FCompressedBuffer& Buffer)
			{
				Builder.AddInput(Key, Buffer);
			});
		}
		return SetInputs(Builder.Build());
	}
	else
	{
		return CompleteWithError(ANSITEXTVIEW("Failed to resolve input data."));
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::EnterExecuteRemote()
{
	if (!EnumHasAnyFlags(BuildPolicy.GetCombinedPolicy() & Context->GetBuildPolicyMask(), EBuildPolicy::BuildRemote) ||
		EnumHasAnyFlags(BuildStatus, EBuildStatus::BuildTryExport))
	{
		return AdvanceToState(EBuildJobState::ResolveInputData);
	}

	if (!Worker || !WorkerExecutor)
	{
		Worker = BuildSystem.GetWorkerRegistry().FindWorker(
			Action.Get().GetFunction(),
			Action.Get().GetFunctionVersion(),
			Action.Get().GetBuildSystemVersion(),
			WorkerExecutor);
	}

	if (!Worker || !WorkerExecutor)
	{
		return AdvanceToState(EBuildJobState::ResolveInputData);
	}
}

void FBuildJob::BeginExecuteRemote()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBuildJob::ExecuteRemote);
	checkf(Worker && WorkerExecutor, TEXT("Job requires a worker in state %s for build of '%s' by %s."),
		LexToString(State), *Name, *WriteToString<32>(FunctionName));
	EnumAddFlags(BuildStatus, EBuildStatus::BuildTryRemote);

	FBuildPolicy RemoteBuildPolicy;
	if (EBuildPolicy Mask = Context->GetBuildPolicyMask(); Mask == ~EBuildPolicy::None)
	{
		RemoteBuildPolicy = BuildPolicy;
	}
	else
	{
		RemoteBuildPolicy = BuildPolicy.Transform([Mask](EBuildPolicy Policy) { return Policy & Mask; });
	}

	WorkerExecutor->Build(Action.Get(), Inputs, RemoteBuildPolicy, *Worker, BuildSystem, Owner,
		[this](FBuildWorkerActionCompleteParams&& Params) { EndExecuteRemote(MoveTemp(Params)); });
}

void FBuildJob::EndExecuteRemote(FBuildWorkerActionCompleteParams&& Params)
{
	if (Params.Status == EStatus::Canceled)
	{
		return CompleteWithError(ANSITEXTVIEW("Build was canceled."));
	}
	if (Params.Output)
	{
		EnumAddFlags(BuildStatus, EBuildStatus::BuildRemote);
		return SetOutputNoCheck(MoveTemp(Params.Output).Get());
	}
	else
	{
		switch (State)
		{
		case EBuildJobState::ExecuteRemote:
		case EBuildJobState::ExecuteRemoteWait:
			MissingInputs = Params.MissingInputs;
			if (!MissingInputs.IsEmpty())
			{
				MissingInputs.Sort();
				return AdvanceToState(EBuildJobState::ResolveRemoteInputData);
			}
			[[fallthrough]];
		case EBuildJobState::ExecuteRemoteRetry:
		case EBuildJobState::ExecuteRemoteRetryWait:
		default:
			return AdvanceToState(EBuildJobState::ResolveInputData);
		}
	}
}

void FBuildJob::SkipExecuteRemote()
{
	checkf(State == EBuildJobState::ResolveRemoteInputData ||
		State == EBuildJobState::ExecuteRemote || State == EBuildJobState::ExecuteRemoteRetry,
		TEXT("Job is not expecting SkipExecuteRemote to be called in state %s for build of '%s' by %s."),
		LexToString(State), *Name, *WriteToString<32>(FunctionName));
	return AdvanceToState(EBuildJobState::ResolveInputData);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::EnterExecuteLocal()
{
	if (EnumHasAnyFlags(BuildStatus, EBuildStatus::BuildTryExport))
	{
		ExportBuild();
		EnumAddFlags(BuildStatus, EBuildStatus::BuildExport);
	}
	const EBuildPolicy LocalBuildPolicy = BuildPolicy.GetCombinedPolicy() & Context->GetBuildPolicyMask();
	if (!EnumHasAnyFlags(LocalBuildPolicy, EBuildPolicy::BuildLocal))
	{
		if (!EnumHasAnyFlags(LocalBuildPolicy, EBuildPolicy::BuildRemote))
		{
			return AdvanceToState(EBuildJobState::Complete);
		}
		else if (EnumHasAnyFlags(BuildStatus, EBuildStatus::BuildTryRemote))
		{
			return CompleteWithError(ANSITEXTVIEW("Failed because build policy does not allow local execution, "
				"and remote execution failed to build."));
		}
		else
		{
			return CompleteWithError(ANSITEXTVIEW("Failed because build policy does not allow local execution, "
				"and remote execution was not available."));
		}
	}

	Action.Get().IterateInputs([this](FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)
	{
		const FCompressedBuffer& Buffer = Inputs.Get().FindInput(Key);
		if (Buffer.GetRawHash() == RawHash && Buffer.GetRawSize() == RawSize)
		{
			Context->AddInput(Key, Buffer);
		}
		else
		{
			CompleteWithError(WriteToUtf8String<256>(ANSITEXTVIEW("Failed because input '"), Key,
				ANSITEXTVIEW("' with hash "), RawHash, ANSITEXTVIEW(" ("), RawSize,
				ANSITEXTVIEW(" bytes) was resolved with hash "), Buffer.GetRawHash(),
				ANSITEXTVIEW(" ("), Buffer.GetRawSize(), ANSITEXTVIEW(" bytes).")));
		}
	});
	Action.Reset();
	Inputs.Reset();
	Schedule->EditParameters().ResolvedInputsSize = 0;
}

void FBuildJob::BeginExecuteLocal()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FBuildJob::ExecuteLocal);
	Context->BeginBuild(Owner, [this] { EndExecuteLocal(); });
}

void FBuildJob::EndExecuteLocal()
{
	EnumAddFlags(BuildStatus, EBuildStatus::BuildLocal);
	return SetOutputNoCheck(OutputBuilder.Build());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::CreateAction(TConstArrayView<FBuildInputMetaByKey> InputMeta)
{
	const FBuildDefinition& LocalDefinition = Definition.Get();
	FBuildActionBuilder Builder = BuildSystem.CreateAction(Name, FunctionName);
	LocalDefinition.IterateConstants([this, &Builder](FUtf8StringView Key, FCbObject&& Value)
	{
		Builder.AddConstant(Key, Value);
	});
	for (const FBuildInputMetaByKey& Input : InputMeta)
	{
		Builder.AddInput(Input.Key, Input.RawHash, Input.RawSize);
	}
	return SetAction(Builder.Build());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::SetDefinition(FBuildDefinition&& InDefinition)
{
	Name = InDefinition.GetName();
	FunctionName = InDefinition.GetFunction();
	checkf(Definition.IsNull(), TEXT("Job already has a definition for build of '%s' by %s."),
		*Name, *WriteToString<32>(FunctionName));
	checkf(State == EBuildJobState::ResolveKey || State == EBuildJobState::ResolveKeyWait,
		TEXT("Job is not expecting a definition in state %s for build of '%s' by %s."),
		LexToString(State), *Name, *WriteToString<32>(FunctionName));
	OutputBuilder = BuildSystem.CreateOutput(Name, FunctionName);
	Definition = MoveTemp(InDefinition);
	return AdvanceToState(EBuildJobState::ResolveInputMeta);
}

void FBuildJob::SetAction(FBuildAction&& InAction)
{
	checkf(Action.IsNull(), TEXT("Job already has an action for build of '%s' by %s."),
		*Name, *WriteToString<32>(FunctionName));
	checkf(State == EBuildJobState::ResolveKey ||
		State == EBuildJobState::ResolveInputMeta || State == EBuildJobState::ResolveInputMetaWait,
		TEXT("Job is not expecting an action in state %s for build of '%s' by %s."),
		LexToString(State), *Name, *WriteToString<32>(FunctionName));
	Action = MoveTemp(InAction);
	return AdvanceToState(EBuildJobState::CacheQuery);
}

void FBuildJob::SetInputs(FBuildInputs&& InInputs)
{
	EBuildJobState ExecuteState;
	switch (State)
	{
	case EBuildJobState::ResolveRemoteInputData:
	case EBuildJobState::ResolveRemoteInputDataWait:
		ExecuteState = EBuildJobState::ExecuteRemoteRetry;
		break;
	case EBuildJobState::ResolveInputData:
	case EBuildJobState::ResolveInputDataWait:
		ExecuteState = EBuildJobState::ExecuteLocal;
		break;
	default:
		checkf(false, TEXT("Job is not expecting inputs in state %s for build of '%s' by %s."),
			LexToString(State), *Name, *WriteToString<32>(FunctionName));
		return;
	}
	Inputs = MoveTemp(InInputs);
	FBuildSchedulerParams& SchedulerParams = Schedule->EditParameters();
	SchedulerParams.MissingLocalInputsSize = 0;
	SchedulerParams.MissingRemoteInputsSize = 0;
	return AdvanceToState(ExecuteState);
}

void FBuildJob::SetOutput(const FBuildOutput& InOutput)
{
	checkf(State == EBuildJobState::ResolveKey || State == EBuildJobState::ResolveInputMeta ||
		State == EBuildJobState::CacheQuery || State == EBuildJobState::ResolveInputData ||
		State == EBuildJobState::ExecuteRemote || State == EBuildJobState::ExecuteRemoteRetry ||
		State == EBuildJobState::ExecuteLocal,
		TEXT("Job is not expecting an output in state %s for build of '%s' by %s."),
		LexToString(State), *Name, *WriteToString<32>(FunctionName));
	return SetOutputNoCheck(FBuildOutput(InOutput));
}

void FBuildJob::SetOutputNoCheck(FBuildOutput&& InOutput, EBuildJobState NewState)
{
	checkf(Output.IsNull(), TEXT("Job already has an output for build of '%s' by %s."), *Name, *WriteToString<32>(FunctionName));
	Output = MoveTemp(InOutput);

	if (OnComplete)
	{
		const FCacheKey& CacheKey = Context ? Context->GetCacheKey() : FCacheKey::Empty;
		const EStatus Status = Owner.IsCanceled() ? EStatus::Canceled : Output.Get().HasError() ? EStatus::Error : EStatus::Ok;
		OnComplete({CacheKey, FBuildOutput(Output.Get()), BuildStatus, Status});
		OnComplete = nullptr;
	}

	return AdvanceToState(NewState);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildJob::CompleteWithError(FUtf8StringView Error)
{
	if (FWriteScopeLock WriteLock(Lock); Output || NextState == EBuildJobState::Complete)
	{
		return;
	}
	OutputBuilder.AddLog({ANSITEXTVIEW("LogDerivedDataBuild"), Error, EBuildOutputLogLevel::Error});
	return SetOutputNoCheck(OutputBuilder.Build(), EBuildJobState::Complete);
}

void FBuildJob::AdvanceToState(EBuildJobState NewState)
{
	EBuildJobState OldState = EBuildJobState::Complete;

	if (FWriteScopeLock WriteLock(Lock); NextState < NewState)
	{
		// TODO: Improve CompleteWithError to avoid unexpected state transitions.
		//checkf(NextState < NewState,
		//	TEXT("Job in state %s is requesting an invalid transition from %s to %s for build of '%s' by %s."),
		//	LexToString(State), LexToString(NextState), LexToString(NewState), *Name, *WriteToString<32>(FunctionName));

		if (Owner.IsCanceled())
		{
			NewState = EBuildJobState::Complete;
			if (Output.IsNull())
			{
				OutputBuilder.AddLog({ANSITEXTVIEW("LogDerivedDataBuild"), ANSITEXTVIEW("Build was canceled."), EBuildOutputLogLevel::Error});
			}
		}

		NextState = NewState;

		if (bInAdvanceToState)
		{
			return;
		}
		else
		{
			bInAdvanceToState = true;
			OldState = State;
			State = NewState;
		}
	}

	while (OldState < NewState)
	{
		ExecuteTransition(OldState, NewState);

		if (NewState == EBuildJobState::Complete)
		{
			return;
		}

		if (FWriteScopeLock WriteLock(Lock); NewState < NextState)
		{
			OldState = NewState;
			NewState = NextState;
			State = NewState;
		}
		else
		{
			bInAdvanceToState = false;
			break;
		}
	}

	if (OldState < NewState)
	{
		ExecuteState(NewState);
	}
}

void FBuildJob::ExecuteTransition(EBuildJobState OldState, EBuildJobState NewState)
{
	if (OldState == EBuildJobState::NotStarted)
	{
		BeginJob();
	}
	if (OldState < EBuildJobState::CacheQuery && EBuildJobState::CacheQuery <= NewState && NewState <= EBuildJobState::CacheStoreWait)
	{
		CreateContext();
	}
	if (OldState <= EBuildJobState::ExecuteRemoteRetryWait && EBuildJobState::ExecuteRemoteRetryWait < NewState)
	{
		Worker = nullptr;
		WorkerExecutor = nullptr;
	}
	if (OldState <= EBuildJobState::ResolveInputDataWait && EBuildJobState::ResolveInputDataWait < NewState)
	{
		MissingInputs.Empty();
	}
	if (OldState <= EBuildJobState::ExecuteLocalWait && EBuildJobState::ExecuteLocalWait < NewState)
	{
		Definition.Reset();
		Action.Reset();
		Inputs.Reset();
		if (Context)
		{
			Context->ResetInputs();
		}
		Schedule->EditParameters().ResolvedInputsSize = 0;
	}
	if (OldState <= EBuildJobState::ExecuteLocalWait && EBuildJobState::ExecuteLocalWait < NewState && !Output)
	{
		SetOutputNoCheck(OutputBuilder.Build(), NewState);
	}
	if (NewState == EBuildJobState::Complete)
	{
		Output.Reset();
		Context = nullptr;
	}

	switch (NewState)
	{
	case EBuildJobState::ResolveKey:                 return EnterResolveKey();
	case EBuildJobState::ResolveKeyWait:             return BeginResolveKey();
	case EBuildJobState::ResolveInputMeta:           return EnterResolveInputMeta();
	case EBuildJobState::ResolveInputMetaWait:       return BeginResolveInputMeta();
	case EBuildJobState::CacheQuery:                 return EnterCacheQuery();
	case EBuildJobState::CacheQueryWait:             return BeginCacheQuery();
	case EBuildJobState::ExecuteRemote:              return EnterExecuteRemote();
	case EBuildJobState::ExecuteRemoteWait:          return BeginExecuteRemote();
	case EBuildJobState::ResolveRemoteInputData:     return EnterResolveInputData();
	case EBuildJobState::ResolveRemoteInputDataWait: return BeginResolveInputData();
	case EBuildJobState::ExecuteRemoteRetry:         return EnterExecuteRemote();
	case EBuildJobState::ExecuteRemoteRetryWait:     return BeginExecuteRemote();
	case EBuildJobState::ResolveInputData:           return EnterResolveInputData();
	case EBuildJobState::ResolveInputDataWait:       return BeginResolveInputData();
	case EBuildJobState::ExecuteLocal:               return EnterExecuteLocal();
	case EBuildJobState::ExecuteLocalWait:           return BeginExecuteLocal();
	case EBuildJobState::CacheStore:                 return EnterCacheStore();
	case EBuildJobState::CacheStoreWait:             return BeginCacheStore();
	case EBuildJobState::Complete:                   return EndJob();
	}
}

void FBuildJob::ExecuteState(EBuildJobState NewState)
{
	switch (NewState)
	{
	case EBuildJobState::ResolveKey:                 return Schedule->ScheduleResolveKey();
	case EBuildJobState::ResolveInputMeta:           return Schedule->ScheduleResolveInputMeta();
	case EBuildJobState::CacheQuery:                 return Schedule->ScheduleCacheQuery();
	case EBuildJobState::ExecuteRemote:              return Schedule->ScheduleExecuteRemote();
	case EBuildJobState::ResolveRemoteInputData:     return Schedule->ScheduleResolveInputData();
	case EBuildJobState::ExecuteRemoteRetry:         return Schedule->ScheduleExecuteRemote();
	case EBuildJobState::ResolveInputData:           return Schedule->ScheduleResolveInputData();
	case EBuildJobState::ExecuteLocal:               return Schedule->ScheduleExecuteLocal();
	case EBuildJobState::CacheStore:                 return Schedule->ScheduleCacheStore();
	}
}

void FBuildJob::ExportBuild() const
{
	// Export to <SavedDir>/DerivedDataBuildExport/<Bucket>[/<Function>]/<Action>
	TStringBuilder<256> ExportPath;
	const FCacheKey& Key = Context->GetCacheKey();
	FPathViews::Append(ExportPath, FPaths::ProjectSavedDir(), TEXT("DerivedDataBuildExport"), Key.Bucket);
	if (!Key.Bucket.ToString().Equals(FunctionName))
	{
		FPathViews::Append(ExportPath, FunctionName);
	}
	FPathViews::Append(ExportPath, Key.Hash);
	int32 ExportRootLen = ExportPath.Len();

	TUtf8StringBuilder<512> Meta;
	Meta << "Name: " << Name << LINE_TERMINATOR_ANSI;
	Meta << "Cache: " << Key << LINE_TERMINATOR_ANSI;
	Meta << "Function: " << FunctionName << LINE_TERMINATOR_ANSI;
	Meta << "FunctionVersion: " << Action.Get().GetFunctionVersion() << LINE_TERMINATOR_ANSI;
	Meta << "BuildSystemVersion: " << Action.Get().GetBuildSystemVersion() << LINE_TERMINATOR_ANSI;
	if (Action.Get().HasConstants())
	{
		Meta << "Constants:" << LINE_TERMINATOR_ANSI;
		Action.Get().IterateConstants([&Meta](FUtf8StringView Key, FCbObject&& Value)
		{
			Meta << "  " << Key << ":" LINE_TERMINATOR_ANSI;
			Meta << "    RawHash: " << FIoHash(Value.GetHash()) << LINE_TERMINATOR_ANSI;
			Meta << "    RawSize: " << Value.GetSize() << LINE_TERMINATOR_ANSI;
		});
	}
	if (Action.Get().HasInputs())
	{
		Meta << "Inputs:" << LINE_TERMINATOR_ANSI;
		Action.Get().IterateInputs([&Meta](FUtf8StringView Key, const FIoHash& RawHash, uint64 RawSize)
		{
			Meta << "  " << Key << ":" LINE_TERMINATOR_ANSI;
			Meta << "    RawHash: " << RawHash << LINE_TERMINATOR_ANSI;
			Meta << "    RawSize: " << RawSize << LINE_TERMINATOR_ANSI;
		});
	}

	FPathViews::Append(ExportPath, TEXT("Meta.yaml"));
	if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileWriter(*ExportPath)})
	{
		Ar->Serialize(Meta.GetData(), Meta.Len());
	}
	ExportPath.RemoveSuffix(ExportPath.Len() - ExportRootLen);

	FPathViews::Append(ExportPath, TEXT("Build.action"));
	if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileWriter(*ExportPath)})
	{
		FCbWriter Writer;
		Action.Get().Save(Writer);
		Writer.Save(*Ar);
	}
	ExportPath.RemoveSuffix(ExportPath.Len() - ExportRootLen);

	if (Inputs)
	{
		Meta << "Inputs:" << LINE_TERMINATOR_ANSI;
		FPathViews::Append(ExportPath, TEXT("Inputs"));
		ExportRootLen = ExportPath.Len();
		Inputs.Get().IterateInputs([&ExportPath, ExportRootLen](FUtf8StringView Key, const FCompressedBuffer& Buffer)
		{
			FPathViews::Append(ExportPath, FIoHash(Buffer.GetRawHash()));
			if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileWriter(*ExportPath)})
			{
				Buffer.Save(*Ar);
			}
			ExportPath.RemoveSuffix(ExportPath.Len() - ExportRootLen);
		});
	}
}

bool FBuildJob::ShouldExportBuild() const
{
	static bool bExportAll;
	static TArray<FName> ExportTypes = ParseExportBuildTypes(bExportAll);
	return bExportAll
		|| Algo::BinarySearch(ExportTypes, FName(FUtf8StringView(FunctionName)), FNameFastLess()) != INDEX_NONE
		|| Algo::BinarySearch(ExportTypes, FName(Context->GetCacheKey().Bucket.ToString()), FNameFastLess()) != INDEX_NONE;
}

TArray<FName> FBuildJob::ParseExportBuildTypes(bool& bOutExportAll)
{
	TArray<FName> ExportTypes;
	if (FString ExportTypesArg; FParse::Value(FCommandLine::Get(), TEXT("-ExportBuilds="), ExportTypesArg))
	{
		String::ParseTokens(ExportTypesArg, TEXT('+'), [&ExportTypes](FStringView Type) { ExportTypes.Emplace(Type); });
		ExportTypes.Sort(FNameFastLess());
		bOutExportAll = false;
	}
	else
	{
		bOutExportAll = FParse::Param(FCommandLine::Get(), TEXT("ExportBuilds"));
	}
	return ExportTypes;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CreateBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildKey& Key,
	const FBuildPolicy& Policy,
	FOnBuildComplete&& OnComplete)
{
	new FBuildJob(Params, Key, Policy, MoveTemp(OnComplete));
}

void CreateBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildDefinition& Definition,
	const FOptionalBuildInputs& Inputs,
	const FBuildPolicy& Policy,
	FOnBuildComplete&& OnComplete)
{
	new FBuildJob(Params, Definition, Inputs, Policy, MoveTemp(OnComplete));
}

void CreateBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildAction& Action,
	const FOptionalBuildInputs& Inputs,
	const FBuildPolicy& Policy,
	FOnBuildComplete&& OnComplete)
{
	new FBuildJob(Params, Action, Inputs, Policy, MoveTemp(OnComplete));
}

} // UE::DerivedData::Private
