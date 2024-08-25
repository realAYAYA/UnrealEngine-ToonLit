// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDisplacedMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMeshSourceData.h"
#include "NaniteDisplacedMeshLog.h"
#include "Engine/StaticMesh.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Rendering/NaniteResources.h"
#include "RenderUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NaniteDisplacedMesh)

#if WITH_EDITOR
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "Experimental/Misc/ExecutionResource.h"
#include "MeshDescriptionHelper.h"
#include "NaniteBuilder.h"
#include "NaniteDisplacedMeshAlgo.h"
#include "NaniteDisplacedMeshCompiler.h"
#include "Serialization/MemoryHasher.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshBuilder.h"
#include "StaticMeshCompiler.h"
#endif


#if WITH_EDITOR

class FNaniteDisplacedMeshAsyncBuildWorker : public FNonAbandonableTask
{
	FNaniteBuildAsyncCacheTask* Owner;
	FIoHash IoHash;
public:
	FNaniteDisplacedMeshAsyncBuildWorker(
		FNaniteBuildAsyncCacheTask* InOwner,
		const FIoHash& InIoHash)
		: Owner(InOwner)
		, IoHash(InIoHash)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FNaniteDisplacedMeshAsyncBuildWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();
};

struct FNaniteDisplacedMeshAsyncBuildTask : public FAsyncTask<FNaniteDisplacedMeshAsyncBuildWorker>
{
	FNaniteDisplacedMeshAsyncBuildTask(
		FNaniteBuildAsyncCacheTask* InOwner,
		const FIoHash& InIoHash)
		: FAsyncTask<FNaniteDisplacedMeshAsyncBuildWorker>(InOwner, InIoHash)
	{
	}
};

class FNaniteBuildAsyncCacheTask
{
public:
	FNaniteBuildAsyncCacheTask(
		const FIoHash& InKeyHash,
		FNaniteData* InData,
		UNaniteDisplacedMesh& InDisplacedMesh,
		const ITargetPlatform* TargetPlatform
	);

	inline void Wait()
	{
		if (bIsWaitingOnMeshCompilation)
		{
			WaitForDependenciesAndBeginCache();
		}

		if (BuildTask != nullptr)
		{
			BuildTask->EnsureCompletion();
		}

		Owner.Wait();
	}

	inline bool Poll()
	{
		if (bIsWaitingOnMeshCompilation)
		{
			BeginCacheIfDependenciesAreFree();
			return false;
		}

		if (BuildTask && !BuildTask->IsDone())
		{
			return false;
		}

		return Owner.Poll();
	}

	inline void Cancel()
	{
		// Cancel the waiting on the static mesh build
		bIsWaitingOnMeshCompilation = false;

		if (BuildTask)
		{
			BuildTask->Cancel();
		}

		Owner.Cancel();
	}

	void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority);

private:
	bool ShouldWaitForBaseMeshCompilation() const;

	void BeginCacheIfDependenciesAreFree();
	void WaitForDependenciesAndBeginCache();

	void BeginCache(const FIoHash& KeyHash);
	void EndCache(UE::DerivedData::FCacheGetValueResponse&& Response);
	bool BuildData(const UE::DerivedData::FSharedString& Name, const UE::DerivedData::FCacheKey& Key);

private:
	friend class FNaniteDisplacedMeshAsyncBuildWorker;
	TUniquePtr<FNaniteDisplacedMeshAsyncBuildTask> BuildTask;
	FNaniteData* Data;
	TWeakObjectPtr<UNaniteDisplacedMesh> WeakDisplacedMesh;

	// Raw for now (The compilation is protected by the NaniteDisplacedMeshEditorSubsystem (todo update this so that it is safe even when the editor subsystem is not present))
	FNaniteDisplacedMeshParams Parameters;

	UE::DerivedData::FRequestOwner Owner;
	TRefCountPtr<IExecutionResource> ExecutionResource;

	bool bIsWaitingOnMeshCompilation;
	FIoHash KeyHash;
};

FNaniteBuildAsyncCacheTask::FNaniteBuildAsyncCacheTask(
	const FIoHash& InKeyHash,
	FNaniteData* InData,
	UNaniteDisplacedMesh& InDisplacedMesh,
	const ITargetPlatform* TargetPlatform
)
	: Data(InData)
	, WeakDisplacedMesh(&InDisplacedMesh)
	, Parameters(InDisplacedMesh.Parameters)
	// Once we pass the BeginCache throttling gate, we want to finish as fast as possible
	// to avoid holding on to memory for a long time. We use the highest priority for all
	// subsequent task.
	, Owner(UE::DerivedData::EPriority::Highest)
	, bIsWaitingOnMeshCompilation(ShouldWaitForBaseMeshCompilation())
	, KeyHash(InKeyHash)
{
	/**
	 * Unfortunately our async builds are not made to handle the assets that use data from other assets
	 * This will delay the start of the actual cache until the build of the base static is done
	 * This will fix a race condition with the static mesh build without blocking the game thread by default.
	 * Note: This is not a perfect solution since it also delay the DDC data pull.
	 */
	if (!bIsWaitingOnMeshCompilation)
	{
		BeginCache(InKeyHash);
	}
}

void FNaniteDisplacedMeshAsyncBuildWorker::DoWork()
{
	using namespace UE::DerivedData;
	if (UNaniteDisplacedMesh* DisplacedMesh = Owner->WeakDisplacedMesh.Get())
	{
		// Grab any execution resources currently assigned to this worker so that we maintain
		// concurrency limit and memory pressure until the whole multi-step task is done.
		Owner->ExecutionResource = FExecutionResourceContext::Get();

		static const FCacheBucket Bucket("NaniteDisplacedMesh");
		GetCache().GetValue({ {{DisplacedMesh->GetPathName()}, {Bucket, IoHash}} }, Owner->Owner,
			  [Task = Owner](FCacheGetValueResponse&& Response) { Task->EndCache(MoveTemp(Response)); });
	}
}

bool FNaniteBuildAsyncCacheTask::ShouldWaitForBaseMeshCompilation() const 
{
	if (UNaniteDisplacedMesh* DisplacedMesh = WeakDisplacedMesh.Get())
	{
		// If the mesh is still waiting for a post load call, let it build it stuff first to avoid blocking the Game Thread
		if (DisplacedMesh->Parameters.BaseMesh->HasAnyFlags(RF_NeedPostLoad))
		{
			return true;
		}

		return DisplacedMesh->Parameters.BaseMesh->IsCompiling();
	}

	return false;
}

void FNaniteBuildAsyncCacheTask::BeginCacheIfDependenciesAreFree()
{
	if (UNaniteDisplacedMesh* DisplacedMesh = WeakDisplacedMesh.Get())
	{
		if (!ShouldWaitForBaseMeshCompilation())
		{
			bIsWaitingOnMeshCompilation = false;
			BeginCache(KeyHash);
		}
	}
	else
	{
		bIsWaitingOnMeshCompilation = false;
	}
}

void FNaniteBuildAsyncCacheTask::WaitForDependenciesAndBeginCache()
{
	if (UNaniteDisplacedMesh* DisplacedMesh = WeakDisplacedMesh.Get())
	{
		if (DisplacedMesh->Parameters.BaseMesh->HasAnyFlags(RF_NeedPostLoad))
		{
			DisplacedMesh->Parameters.BaseMesh->ConditionalPostLoad();
		}

		FStaticMeshCompilingManager::Get().FinishCompilation({DisplacedMesh->Parameters.BaseMesh});

		bIsWaitingOnMeshCompilation = false;
		BeginCache(KeyHash);
	}
	else
	{
		bIsWaitingOnMeshCompilation = false;
	}

}

void FNaniteBuildAsyncCacheTask::BeginCache(const FIoHash& InKeyHash)
{
	using namespace UE::DerivedData;

	if (UNaniteDisplacedMesh* DisplacedMesh = WeakDisplacedMesh.Get())
	{
		// Queue this launch through the thread pool so that we benefit from fair scheduling and memory throttling
		FQueuedThreadPool* ThreadPool = FNaniteDisplacedMeshCompilingManager::Get().GetThreadPool();
		EQueuedWorkPriority BasePriority = FNaniteDisplacedMeshCompilingManager::Get().GetBasePriority(DisplacedMesh);

		// TODO DC Use the default for now but provide a better estimate for memory usage of displacement build.
		int64 RequiredMemory = -1; // @todo RequiredMemory

		check(BuildTask == nullptr);
		BuildTask = MakeUnique<FNaniteDisplacedMeshAsyncBuildTask>(this, InKeyHash);
		BuildTask->StartBackgroundTask(ThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait, RequiredMemory, TEXT("NaniteDisplacedMesh"));
	}
}

void FNaniteBuildAsyncCacheTask::EndCache(UE::DerivedData::FCacheGetValueResponse&& Response)
{
	using namespace UE::DerivedData;

	if (Response.Status == EStatus::Ok)
	{
		Owner.LaunchTask(TEXT("NaniteDisplacedMeshSerialize"), [this, Value = MoveTemp(Response.Value)]
		{
			// Release execution resource as soon as the task is done
			ON_SCOPE_EXIT { ExecutionResource = nullptr; };

			if (UNaniteDisplacedMesh* DisplacedMesh = WeakDisplacedMesh.Get())
			{
				FSharedBuffer RecordData = Value.GetData().Decompress();
				FMemoryReaderView Ar(RecordData, /*bIsPersistent*/ true);
				Data->ResourcesPtr->Serialize(Ar, DisplacedMesh, /*bCooked*/ false);
				Ar << Data->MeshSections;

				// The initialization of the resources is done by FNaniteDisplacedMeshCompilingManager to avoid race conditions
			}
		});
	}
	else if (Response.Status == EStatus::Error)
	{
		Owner.LaunchTask(TEXT("NaniteDisplacedMeshBuild"), [this, Name = Response.Name, Key = Response.Key]
		{
			// Release execution resource as soon as the task is done
			ON_SCOPE_EXIT { ExecutionResource = nullptr; };

			if (!BuildData(Name, Key))
			{
				return;
			}
			if (UNaniteDisplacedMesh* DisplacedMesh = WeakDisplacedMesh.Get())
			{
				InitNaniteResources(Data->ResourcesPtr);

				TArray64<uint8> RecordData;
				FMemoryWriter64 Ar(RecordData, /*bIsPersistent*/ true);
				Data->ResourcesPtr->Serialize(Ar, DisplacedMesh, /*bCooked*/ false);
				Ar << Data->MeshSections;

				GetCache().PutValue({ {Name, Key, FValue::Compress(MakeSharedBufferFromArray(MoveTemp(RecordData)))} }, Owner);

				// The initialization of the resources is done by FNaniteDisplacedMeshCompilingManager to avoid race conditions
			}
		});
	}
	else
	{
		// Release execution resource as soon as the task is done
		ExecutionResource = nullptr;
	}
}

static FStaticMeshSourceModel& GetBaseMeshSourceModel(UStaticMesh& BaseMesh)
{
	const bool bHasHiResSourceModel = BaseMesh.IsHiResMeshDescriptionValid();
	return bHasHiResSourceModel ? BaseMesh.GetHiResSourceModel() : BaseMesh.GetSourceModel(0);
}

bool FNaniteBuildAsyncCacheTask::BuildData(const UE::DerivedData::FSharedString& Name, const UE::DerivedData::FCacheKey& Key)
{
	using namespace UE::DerivedData;

	UNaniteDisplacedMesh* DisplacedMesh = WeakDisplacedMesh.Get();
	if (!DisplacedMesh)
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FNaniteBuildAsyncCacheTask::BuildData);

	Nanite::IBuilderModule& NaniteBuilderModule = Nanite::IBuilderModule::Get();

	ClearNaniteResources(Data->ResourcesPtr);
	Data->MeshSections.Empty();

	UStaticMesh* BaseMesh = Parameters.BaseMesh;

	if (!IsValid(BaseMesh))
	{
		UE_LOG(LogNaniteDisplacedMesh, Error, TEXT("Cannot find a valid base mesh to build the displaced mesh asset."));
		return false;
	}

	if (!BaseMesh->IsMeshDescriptionValid(0))
	{
		UE_LOG(LogNaniteDisplacedMesh, Error, TEXT("Cannot find a valid mesh description to build the displaced mesh asset."));
		return false;
	}

	FStaticMeshSourceModel& SourceModel = GetBaseMeshSourceModel(*BaseMesh);
	
	FMeshDescription MeshDescription; 
	if (!SourceModel.CloneMeshDescription(MeshDescription))
	{
		UE_LOG(LogNaniteDisplacedMesh, Error, TEXT("Cannot find a valid mesh description to build the displaced mesh asset."));
		return false;
	}

	// Note: We intentionally ignore BaseMesh->NaniteSettings so we don't couple against a mesh that may
	// not ever render as Nanite directly. It is expected that anyone using a Nanite displaced mesh asset
	// will always want Nanite unless the platform, runtime, or "Disallow Nanite" on SMC prevents it.
	FMeshNaniteSettings NaniteSettings;
	NaniteSettings.bEnabled = true;
	NaniteSettings.bExplicitTangents = BaseMesh->NaniteSettings.bExplicitTangents;	// TODO: Expose directly instead of inheriting from base mesh?
	NaniteSettings.TrimRelativeError = Parameters.RelativeError;

	FMeshBuildSettings& BuildSettings = SourceModel.BuildSettings;
	FMeshDescriptionHelper MeshDescriptionHelper(&BuildSettings);
	MeshDescriptionHelper.SetupRenderMeshDescription(BaseMesh, MeshDescription, true, NaniteSettings.bExplicitTangents);

	const FMeshSectionInfoMap BeforeBuildSectionInfoMap = BaseMesh->GetSectionInfoMap();
	const FMeshSectionInfoMap BeforeBuildOriginalSectionInfoMap = BaseMesh->GetOriginalSectionInfoMap();

	const int32 NumSourceModels = BaseMesh->GetNumSourceModels();

	TArray<FMeshDescription> MeshDescriptions;
	MeshDescriptions.SetNum(NumSourceModels);

	Nanite::IBuilderModule::FInputMeshData InputMeshData;

	TArray<int32> RemapVerts;
	TArray<int32> WedgeMap;

	TArray<TArray<uint32>> PerSectionIndices;
	PerSectionIndices.AddDefaulted(MeshDescription.PolygonGroups().Num());
	InputMeshData.Sections.Empty(MeshDescription.PolygonGroups().Num());

	FBoxSphereBounds MeshBounds;

	UE::Private::StaticMeshBuilder::BuildVertexBuffer(
		BaseMesh,
		MeshDescription,
		BuildSettings,
		WedgeMap,
		InputMeshData.Sections,
		PerSectionIndices,
		InputMeshData.Vertices,
		MeshDescriptionHelper.GetOverlappingCorners(),
		RemapVerts,
		MeshBounds,
		NaniteSettings.bExplicitTangents /* bNeedTangents */,
		false /* bNeedWedgeMap */
	);

	if (Owner.IsCanceled())
	{
		return false;
	}

	const uint32 NumTextureCoord = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate).GetNumChannels();

	// Only the render data and vertex buffers will be used from now on unless we have more than one source models
	// This will help with memory usage for Nanite Mesh by releasing memory before doing the build
	MeshDescription.Empty();

	TArray<uint32> CombinedIndices;
	bool bNeeds32BitIndices = false;
	UE::Private::StaticMeshBuilder::BuildCombinedSectionIndices(
		PerSectionIndices,
		InputMeshData.Sections,
		InputMeshData.TriangleIndices,
		bNeeds32BitIndices
	);

	if (Owner.IsCanceled())
	{
		return false;
	}

	// Nanite build requires the section material indices to have already been resolved from the SectionInfoMap
	// as the indices are baked into the FMaterialTriangles.
	for (int32 SectionIndex = 0; SectionIndex < InputMeshData.Sections.Num(); SectionIndex++)
	{
		InputMeshData.Sections[SectionIndex].MaterialIndex = BaseMesh->GetSectionInfoMap().Get(0, SectionIndex).MaterialIndex;
	}
	
	{
		InputMeshData.MaterialIndices.Reserve( InputMeshData.TriangleIndices.Num() / 3 );

		for (FStaticMeshSection& Section : InputMeshData.Sections)
		{
			if (Section.NumTriangles > 0)
			{
				Data->MeshSections.Add(Section);
			}

			for( uint32 i = 0; i < Section.NumTriangles; i++ )
				InputMeshData.MaterialIndices.Add( Section.MaterialIndex );
		}
	}

	// Perform displacement mapping against base mesh using supplied parameterization
	if (!DisplaceNaniteMesh(
			Parameters,
			NumTextureCoord,
			InputMeshData.Vertices,
			InputMeshData.TriangleIndices,
			InputMeshData.MaterialIndices,
			InputMeshData.VertexBounds)
		)
	{
		UE_LOG(LogNaniteDisplacedMesh, Error, TEXT("Failed to build perform displacement mapping for Nanite displaced mesh asset."));
		return false;
	}

	if (Owner.IsCanceled())
	{
		return false;
	}

	InputMeshData.TriangleCounts.Add(InputMeshData.TriangleIndices.Num() / 3);
	InputMeshData.NumTexCoords = NumTextureCoord;

	auto OnFreeInputMeshData = Nanite::IBuilderModule::FOnFreeInputMeshData::CreateLambda([&InputMeshData](bool bFallbackIsReduced)
	{
		if (bFallbackIsReduced)
		{
			InputMeshData.Vertices.Empty();
			InputMeshData.TriangleIndices.Empty();
		}

		InputMeshData.MaterialIndices.Empty();
	});

	TArrayView<Nanite::IBuilderModule::FOutputMeshData> OutputLODMeshData;

	// Pass displaced mesh over to Nanite to build the bulk data
	if (!NaniteBuilderModule.Build(
			*Data->ResourcesPtr.Get(),
			InputMeshData,
			OutputLODMeshData,
			NaniteSettings,
			OnFreeInputMeshData)
		)
	{
		UE_LOG(LogNaniteDisplacedMesh, Error, TEXT("Failed to build Nanite for displaced mesh asset."));
		return false;
	}

	if (Owner.IsCanceled())
	{
		return false;
	}

	return true;
}

void FNaniteBuildAsyncCacheTask::Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority)
{
	if (BuildTask)
	{
		BuildTask->Reschedule(InThreadPool, InPriority);
	}
}

bool FNaniteDisplacedMeshParams::IsDisplacementRequired() const
{
	// We always need a valid base mesh for displacement, and non-zero magnitude on at least one displacement map
	bool bApplyDisplacement = false;
	for (auto& DisplacementMap : DisplacementMaps)
	{
		bApplyDisplacement = bApplyDisplacement || (DisplacementMap.Magnitude > 0.0f && IsValid(DisplacementMap.Texture));
	}

	if (!IsValid(BaseMesh) || !bApplyDisplacement || RelativeError <= 0.0f)
	{
		return false;
	}

	return true;
}

UNaniteDisplacedMesh::FOnNaniteDisplacmentMeshDependenciesChanged UNaniteDisplacedMesh::OnDependenciesChanged;

#endif


UNaniteDisplacedMesh::UNaniteDisplacedMesh(const FObjectInitializer& Init)
: Super(Init)
{
	ClearNaniteResources(Data.ResourcesPtr);
}

void UNaniteDisplacedMesh::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsFilterEditorOnly() && !Ar.IsObjectReferenceCollector() && !Ar.ShouldSkipBulkData())
	{
	#if WITH_EDITOR
		if (Ar.IsCooking())
		{
			if (IsCompiling())
			{
				FNaniteDisplacedMeshCompilingManager::Get().FinishCompilation({this});
			}

			FNaniteData& CookedData = CacheDerivedData(Ar.CookingTarget());
			CookedData.ResourcesPtr->Serialize(Ar, this, /*bCooked*/ true);
			Ar << CookedData.MeshSections;
		}
		else
	#endif
		{
			InitNaniteResources(Data.ResourcesPtr);
			Data.ResourcesPtr->Serialize(Ar, this, /*bCooked*/ true);
			Ar << Data.MeshSections;
		}
	}
}

void UNaniteDisplacedMesh::PostLoad()
{
	InitNaniteResources(Data.ResourcesPtr);

	if (FApp::CanEverRender())
	{
		// Only valid for cooked builds
		if (Data.ResourcesPtr->PageStreamingStates.Num() > 0)
		{
			InitResources();
		}
	#if WITH_EDITOR
		else if (ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform())
		{
			BeginCacheDerivedData(RunningPlatform);
		}
	#endif
	}

	#if WITH_EDITOR
		OnDependenciesChanged.Broadcast(this);
	#endif

	Super::PostLoad();
}

void UNaniteDisplacedMesh::BeginDestroy()
{
	Super::BeginDestroy();

	ReleaseResources();

	#if WITH_EDITOR
		OnDependenciesChanged.Broadcast(this);
	#endif
}

bool UNaniteDisplacedMesh::IsReadyForFinishDestroy()
{
	if (!Super::IsReadyForFinishDestroy())
	{
		return false;
	}

#if WITH_EDITOR
	if (!TryCancelAsyncTasks())
	{
		return false;
	}
#endif

	return ReleaseResourcesFence.IsFenceComplete();
}

bool UNaniteDisplacedMesh::NeedsLoadForTargetPlatform(const ITargetPlatform* TargetPlatform) const
{
	return DoesTargetPlatformSupportNanite(TargetPlatform);
}

void UNaniteDisplacedMesh::InitResources()
{
	if (!FApp::CanEverRender())
	{
		return;
	}

	if (!bIsInitialized)
	{
		Data.ResourcesPtr->InitResources(this);
		bIsInitialized = true;
	}
}

void UNaniteDisplacedMesh::ReleaseResources()
{
	if (!bIsInitialized)
	{
		return;
	}

	if (Data.ResourcesPtr->ReleaseResources())
	{
		// Make sure the renderer is done processing the command,
		// and done using the Nanite resources before we overwrite the data.
		ReleaseResourcesFence.BeginFence();
	}

	bIsInitialized = false;
}

bool UNaniteDisplacedMesh::HasValidNaniteData() const
{
	return bIsInitialized && Data.ResourcesPtr->PageStreamingStates.Num() > 0;
}

#if WITH_EDITOR

void UNaniteDisplacedMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnDependenciesChanged.Broadcast(this);

	// TODO: Add delegates for begin and end build events to safely reload scene proxies, etc.

	// Synchronously build the new data. This calls InitResources.
	ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	BeginCacheDerivedData(RunningPlatform);
}

void UNaniteDisplacedMesh::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);
	BeginCacheDerivedData(TargetPlatform);
}

bool UNaniteDisplacedMesh::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	const FIoHash KeyHash = CreateDerivedDataKeyHash(TargetPlatform);
	if (KeyHash.IsZero())
	{
		return true;
	}

	if (PollCacheDerivedData(KeyHash))
	{
		EndCacheDerivedData(KeyHash);
		return true;
	}

	return false;
}

void UNaniteDisplacedMesh::ClearAllCachedCookedPlatformData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UNaniteDisplacedMesh::ClearAllCachedCookedPlatformData);

	// This is not ideal because we must wait for the tasks to finish or be canceled. They might work with an ptr to the FNaniteData contained in the DataByPlatformKeyHash map and we can't safely disarm them at moment. 
	if (!TryCancelAsyncTasks())
	{
		FinishAsyncTasks();
	}

	/**
	 * TryCancelAsyncTasks or FinishAsyncTasks should have been able to clear all tasks.If any tasks remain
	 * then they must still be running, and we would crash when attempting to delete them.
	 */
	check(CacheTasksByKeyHash.IsEmpty()); 

	DataByPlatformKeyHash.Empty();
	Super::ClearAllCachedCookedPlatformData();
}

FDelegateHandle UNaniteDisplacedMesh::RegisterOnRenderingDataChanged(const FOnRebuild& Delegate)
{
	return OnRenderingDataChanged.Add(Delegate);
}

void UNaniteDisplacedMesh::UnregisterOnRenderingDataChanged(void* Unregister)
{
	OnRenderingDataChanged.RemoveAll(Unregister);
}

void UNaniteDisplacedMesh::UnregisterOnRenderingDataChanged(FDelegateHandle Handle)
{
	OnRenderingDataChanged.Remove(Handle);
}

void UNaniteDisplacedMesh::NotifyOnRenderingDataChanged()
{
	OnRenderingDataChanged.Broadcast();
}

FIoHash UNaniteDisplacedMesh::CreateDerivedDataKeyHash(const ITargetPlatform* TargetPlatform)
{
	if (!DoesTargetPlatformSupportNanite(TargetPlatform))
	{
		return FIoHash::Zero;
	}

	FMemoryHasherBlake3 Writer;

	FGuid DisplacedMeshVersionGuid(0x9725551B, 0xF79443C1, 0x84F3ED2D, 0xD65499BA);
	Writer << DisplacedMeshVersionGuid;

	FGuid NaniteVersionGuid = FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().NANITE_DERIVEDDATA_VER);
	Writer << NaniteVersionGuid;

	const FStaticMeshLODSettings& PlatformLODSettings = TargetPlatform->GetStaticMeshLODSettings();

	if (IsValid(Parameters.BaseMesh))
	{
		const FStaticMeshLODGroup& LODGroup = PlatformLODSettings.GetLODGroup(Parameters.BaseMesh->LODGroup);
		FString StaticMeshKey = UE::Private::StaticMesh::BuildStaticMeshDerivedDataKey(TargetPlatform, Parameters.BaseMesh, LODGroup);
		Writer << StaticMeshKey;
	}

	Writer << Parameters.RelativeError;

	for( auto& DisplacementMap : Parameters.DisplacementMaps )
	{
		if (IsValid(DisplacementMap.Texture))
		{
			FGuid TextureId = DisplacementMap.Texture->Source.GetId();
			Writer << TextureId;
			Writer << DisplacementMap.Texture->AddressX;
			Writer << DisplacementMap.Texture->AddressY;
		}

		Writer << DisplacementMap.Magnitude;
		Writer << DisplacementMap.Center;
	}

#if PLATFORM_CPU_ARM_FAMILY
	// Separate out arm keys as x64 and arm64 clang do not generate the same data for a given
	// input. Add the arm specifically so that a) we avoid rebuilding the current DDC and
	// b) we can remove it once we get arm64 to be consistent.
	FString ArmSuffix(TEXT("_arm64"));
	Writer << ArmSuffix;
#endif

	return Writer.Finalize();
}

FIoHash UNaniteDisplacedMesh::BeginCacheDerivedData(const ITargetPlatform* TargetPlatform)
{
	const FIoHash KeyHash = CreateDerivedDataKeyHash(TargetPlatform);

	if (KeyHash.IsZero() || DataKeyHash == KeyHash || DataByPlatformKeyHash.Contains(KeyHash))
	{
		return KeyHash;
	}

	// Make sure we finish the previous build before starting another one
	UNaniteDisplacedMesh* LValueThis = this;
	FNaniteDisplacedMeshCompilingManager::Get().FinishCompilation(TArrayView<UNaniteDisplacedMesh* const>(&LValueThis, 1));

	// Make sure the GPU is no longer referencing the current Nanite resource data.
	ReleaseResources();
	ReleaseResourcesFence.Wait();
	ClearNaniteResources(Data.ResourcesPtr);
	Data.MeshSections.Empty();

	NotifyOnRenderingDataChanged();

	FNaniteData* TargetData = nullptr;
	if (TargetPlatform->IsRunningPlatform())
	{
		DataKeyHash = KeyHash;
		TargetData = &Data;
	}
	else
	{
		TargetData = DataByPlatformKeyHash.Emplace(KeyHash, MakeUnique<FNaniteData>()).Get();
	}

	InitNaniteResources(TargetData->ResourcesPtr);
	CacheTasksByKeyHash.Emplace(KeyHash, MakePimpl<FNaniteBuildAsyncCacheTask>(KeyHash, TargetData, *this, TargetPlatform));

	// The compiling manager provides throttling, notification manager, etc... for the asset being built.
	FNaniteDisplacedMeshCompilingManager::Get().AddNaniteDisplacedMeshes({this});

	return KeyHash;
}

void UNaniteDisplacedMesh::FinishAsyncTasks()
{
	for (auto It = CacheTasksByKeyHash.CreateIterator(); It; ++It)
	{
		It->Value->Wait();
		It.RemoveCurrent();
	}
}

bool UNaniteDisplacedMesh::IsCompiling() const
{
	return CacheTasksByKeyHash.Num() > 0;
}

bool UNaniteDisplacedMesh::TryCancelAsyncTasks()
{
	for (auto It = CacheTasksByKeyHash.CreateIterator(); It; ++It)
	{
		if (It->Value->Poll())
		{
			It.RemoveCurrent();
		}
		else
		{
			It->Value->Cancel();

			// Try to see if we can remove the task now that it might have been canceled
			if (It->Value->Poll())
			{
				It.RemoveCurrent();
			}
		}
	}
	
	return CacheTasksByKeyHash.IsEmpty();
}

bool UNaniteDisplacedMesh::IsAsyncTaskComplete() const
{
	for (auto& Pair : CacheTasksByKeyHash)
	{
		if (!Pair.Value->Poll())
		{
			return false;
		}
	}

	return true;
}

void UNaniteDisplacedMesh::Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority)
{
	for (auto& Pair : CacheTasksByKeyHash)
	{
		Pair.Value->Reschedule(InThreadPool, InPriority);
	}
}

bool UNaniteDisplacedMesh::PollCacheDerivedData(const FIoHash& KeyHash) const
{
	if (KeyHash.IsZero())
	{
		return true;
	}

	if (const TPimplPtr<FNaniteBuildAsyncCacheTask>* Task = CacheTasksByKeyHash.Find(KeyHash))
	{
		return (*Task)->Poll();
	}

	return true;
}

void UNaniteDisplacedMesh::EndCacheDerivedData(const FIoHash& KeyHash)
{
	if (KeyHash.IsZero())
	{
		return;
	}

	TPimplPtr<FNaniteBuildAsyncCacheTask> Task;
	if (CacheTasksByKeyHash.RemoveAndCopyValue(KeyHash, Task))
	{
		Task->Wait();
	}
}

FNaniteData& UNaniteDisplacedMesh::CacheDerivedData(const ITargetPlatform* TargetPlatform)
{
	const FIoHash KeyHash = BeginCacheDerivedData(TargetPlatform);
	EndCacheDerivedData(KeyHash);
	FNaniteData& NaniteData = (DataKeyHash == KeyHash) ? Data : *DataByPlatformKeyHash[KeyHash];
	InitNaniteResources(NaniteData.ResourcesPtr);
	return NaniteData;
}

#endif // WITH_EDITOR

