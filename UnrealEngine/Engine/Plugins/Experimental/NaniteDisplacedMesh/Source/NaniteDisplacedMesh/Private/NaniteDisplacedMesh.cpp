// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDisplacedMesh.h"
#include "NaniteDisplacedMeshLog.h"
#include "Modules/ModuleManager.h"
#include "Engine/StaticMesh.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NaniteDisplacedMesh)

#if WITH_EDITOR
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "Serialization/MemoryHasher.h"
#include "Async/Async.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshBuilder.h"
#include "Experimental/Misc/ExecutionResource.h"
#include "MeshDescriptionHelper.h"
#include "NaniteBuilder.h"
#include "NaniteDisplacedMeshAlgo.h"
#include "NaniteDisplacedMeshCompiler.h"
#endif

static bool DoesTargetPlatformSupportNanite(const ITargetPlatform* TargetPlatform)
{
	if (TargetPlatform != nullptr)
	{
		TArray<FName> DesiredShaderFormats;
		TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);
		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);
			if (DoesPlatformSupportNanite(ShaderPlatform))
			{
				return true;
			}
		}
	}

	return false;
}

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
		if (BuildTask != nullptr)
		{
			BuildTask->EnsureCompletion();
		}

		Owner.Wait();
	}

	inline bool Poll() const
	{
		if (BuildTask && !BuildTask->IsDone())
		{
			return false;
		}

		return Owner.Poll();
	}

	inline void Cancel()
	{ 
		if (BuildTask)
		{
			BuildTask->Cancel();
		}

		Owner.Cancel();
	}

	void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority);

private:
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
{
	BeginCache(InKeyHash);
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

void FNaniteBuildAsyncCacheTask::BeginCache(const FIoHash& KeyHash)
{
	using namespace UE::DerivedData;

	if (UNaniteDisplacedMesh* DisplacedMesh = WeakDisplacedMesh.Get())
	{
		// Queue this launch through the thread pool so that we benefit from fair scheduling and memory throttling
		FQueuedThreadPool* ThreadPool = FNaniteDisplacedMeshCompilingManager::Get().GetThreadPool();
		EQueuedWorkPriority BasePriority = FNaniteDisplacedMeshCompilingManager::Get().GetBasePriority(DisplacedMesh);

		// TODO DC Use the default for now but provide a better estimate for memory usage of displacement build.
		int64 RequiredMemory = -1;

		check(BuildTask == nullptr);
		BuildTask = MakeUnique<FNaniteDisplacedMeshAsyncBuildTask>(this, KeyHash);
		BuildTask->StartBackgroundTask(ThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait, RequiredMemory);
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
				Data->Resources.Serialize(Ar, DisplacedMesh, /*bCooked*/ false);
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
				TArray64<uint8> RecordData;
				FMemoryWriter64 Ar(RecordData, /*bIsPersistent*/ true);
				Data->Resources.Serialize(Ar, DisplacedMesh, /*bCooked*/ false);
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

	Data->Resources = {};
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
	
	FMeshDescription MeshDescription = *SourceModel.GetOrCacheMeshDescription();

	FMeshBuildSettings& BuildSettings = SourceModel.BuildSettings;
	FMeshDescriptionHelper MeshDescriptionHelper(&BuildSettings);
	MeshDescriptionHelper.SetupRenderMeshDescription(BaseMesh, MeshDescription, true, false);

	const FMeshSectionInfoMap BeforeBuildSectionInfoMap = BaseMesh->GetSectionInfoMap();
	const FMeshSectionInfoMap BeforeBuildOriginalSectionInfoMap = BaseMesh->GetOriginalSectionInfoMap();

	// Note: We intentionally ignore BaseMesh->NaniteSettings so we don't couple against a mesh that may
	// not ever render as Nanite directly. It is expected that anyone using a Nanite displaced mesh asset
	// will always want Nanite unless the platform, runtime, or "Disallow Nanite" on SMC prevents it.
	FMeshNaniteSettings NaniteSettings;
	NaniteSettings.bEnabled = true;
	NaniteSettings.TrimRelativeError = Parameters.RelativeError;

	const int32 NumSourceModels = BaseMesh->GetNumSourceModels();

	TArray<FMeshDescription> MeshDescriptions;
	MeshDescriptions.SetNum(NumSourceModels);

	Nanite::IBuilderModule::FVertexMeshData InputMeshData;

	TArray<int32> RemapVerts;
	TArray<int32> WedgeMap;

	TArray<TArray<uint32>> PerSectionIndices;
	PerSectionIndices.AddDefaulted(MeshDescription.PolygonGroups().Num());
	InputMeshData.Sections.Empty(MeshDescription.PolygonGroups().Num());

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
		false
	);

	if (Owner.IsCanceled())
	{
		return false;
	}

	const uint32 NumTextureCoord = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector2f>(MeshAttribute::VertexInstance::TextureCoordinate).GetNumChannels();

	// Make sure to not keep the large WedgeMap from the input mesh around.
	WedgeMap.Empty();

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
	
	TArray< int32 > MaterialIndexes;
	{
		MaterialIndexes.Reserve( InputMeshData.TriangleIndices.Num() / 3 );

		for (FStaticMeshSection& Section : InputMeshData.Sections)
		{
			if (Section.NumTriangles > 0)
			{
				Data->MeshSections.Add(Section);
			}

			for( uint32 i = 0; i < Section.NumTriangles; i++ )
				MaterialIndexes.Add( Section.MaterialIndex );
		}
	}

	// Perform displacement mapping against base mesh using supplied parameterization
	if (!DisplaceNaniteMesh(
			Parameters,
			NumTextureCoord,
			InputMeshData.Vertices,
			InputMeshData.TriangleIndices,
			MaterialIndexes )
		)
	{
		UE_LOG(LogNaniteDisplacedMesh, Error, TEXT("Failed to build perform displacement mapping for Nanite displaced mesh asset."));
		return false;
	}

	if (Owner.IsCanceled())
	{
		return false;
	}

	// Compute mesh bounds after displacement has run
	// TODO: Do we need this? The base mesh bounds will not exactly match the displaced mesh bounds (but cluster bounds will be correct).
	//FBoxSphereBounds MeshBounds;
	//ComputeBoundsFromVertexList(InputMeshData.Vertices, MeshBounds);

	TArray<uint32> MeshTriangleCounts;
	MeshTriangleCounts.Add(InputMeshData.TriangleIndices.Num() / 3);

	// Pass displaced mesh over to Nanite to build the bulk data
	if (!NaniteBuilderModule.Build(
			Data->Resources,
			InputMeshData.Vertices,
			InputMeshData.TriangleIndices,
			MaterialIndexes,
			MeshTriangleCounts,
			NumTextureCoord,
			NaniteSettings)
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
			CookedData.Resources.Serialize(Ar, this, /*bCooked*/ true);
			Ar << CookedData.MeshSections;
		}
		else
	#endif
		{
			Data.Resources.Serialize(Ar, this, /*bCooked*/ true);
			Ar << Data.MeshSections;
		}
	}
}

void UNaniteDisplacedMesh::PostLoad()
{
	if (FApp::CanEverRender())
	{
		// Only valid for cooked builds
		if (Data.Resources.PageStreamingStates.Num() > 0)
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
	return IsSupportedByTargetPlatform(TargetPlatform);
}

bool UNaniteDisplacedMesh::IsSupportedByTargetPlatform(const ITargetPlatform* TargetPlatform)
{
	if (TargetPlatform != nullptr)
	{
		TArray<FName> DesiredShaderFormats;
		TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);
		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);
			if (DoesPlatformSupportNanite(ShaderPlatform))
			{
				return true;
			}
		}
	}

	return false;
}

void UNaniteDisplacedMesh::InitResources()
{
	if (!FApp::CanEverRender())
	{
		return;
	}

	if (!bIsInitialized)
	{
		Data.Resources.InitResources(this);

		bIsInitialized = true;
	}
}

void UNaniteDisplacedMesh::ReleaseResources()
{
	if (!bIsInitialized)
	{
		return;
	}

	if (Data.Resources.ReleaseResources())
	{
		// Make sure the renderer is done processing the command,
		// and done using the Nanite resources before we overwrite the data.
		ReleaseResourcesFence.BeginFence();
	}

	bIsInitialized = false;
}

bool UNaniteDisplacedMesh::HasValidNaniteData() const
{
	return bIsInitialized && Data.Resources.PageStreamingStates.Num() > 0;
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
	// Delete any cache tasks first because the destructor will cancel the cache and build tasks,
	// and drop their pointers to the data.
	CacheTasksByKeyHash.Empty();
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
	if (!IsSupportedByTargetPlatform(TargetPlatform))
	{
		return FIoHash::Zero;
	}

	FMemoryHasherBlake3 Writer;

	FGuid DisplacedMeshVersionGuid(0x5E7DB989, 0x619E4CCA, 0x88D133BE, 0x2B847F10);
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
	Data.Resources = {};
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
	return DataKeyHash == KeyHash ? Data : *DataByPlatformKeyHash[KeyHash];
}

#endif // WITH_EDITOR

