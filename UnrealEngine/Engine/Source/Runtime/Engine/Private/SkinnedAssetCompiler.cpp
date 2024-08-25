// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkinnedAssetCompiler.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/SkeletalMesh.h" // For AddSkeletalMeshes
#include "UObject/UnrealType.h"

#if WITH_EDITOR

#include "AssetCompilingManager.h"
#include "AsyncCompilationHelpers.h"
#include "EngineLogs.h"
#include "ObjectCacheContext.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "Engine/SkinnedAssetAsyncCompileUtils.h"
#include "UObject/StrongObjectPtr.h"
#include "ShaderCompiler.h"
#include "TextureCompiler.h"
#include "ProfilingDebugging/CountersTrace.h"

#define LOCTEXT_NAMESPACE "SkinnedAssetCompiler"

static AsyncCompilationHelpers::FAsyncCompilationStandardCVars CVarAsyncSkinnedAssetStandard(
	TEXT("SkinnedAsset"),
	TEXT("skinned assets"),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			FSkinnedAssetCompilingManager::Get().FinishAllCompilation();
		}
	));

namespace SkinnedAssetCompilingManagerImpl
{
	static void EnsureInitializedCVars()
	{
		static bool bIsInitialized = false;

		if (!bIsInitialized)
		{
			bIsInitialized = true;
			
			AsyncCompilationHelpers::EnsureInitializedCVars(
				TEXT("skinnedasset"),
				CVarAsyncSkinnedAssetStandard.AsyncCompilation,
				CVarAsyncSkinnedAssetStandard.AsyncCompilationMaxConcurrency,
				GET_MEMBER_NAME_CHECKED(UEditorExperimentalSettings, bEnableAsyncSkinnedAssetCompilation));
		}
	}
}

FSkinnedAssetCompilingManager::FSkinnedAssetCompilingManager()
	: Notification(MakeUnique<FAsyncCompilationNotification>(GetAssetNameFormat()))
{
	SkinnedAssetCompilingManagerImpl::EnsureInitializedCVars();
	PostReachabilityAnalysisHandle = FCoreUObjectDelegates::PostReachabilityAnalysis.AddRaw(this, &FSkinnedAssetCompilingManager::OnPostReachabilityAnalysis);
	PreGarbageCollectHandle = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FSkinnedAssetCompilingManager::OnPreGarbageCollect);
}

FName FSkinnedAssetCompilingManager::GetAssetTypeName() const
{
	return TEXT("UE-SkinnedAsset");
}

FTextFormat FSkinnedAssetCompilingManager::GetAssetNameFormat() const
{
	return LOCTEXT("SkinnedAssetNameFormat", "{0}|plural(one=Skinned Asset,other=Skinned Assets)");
}

TArrayView<FName> FSkinnedAssetCompilingManager::GetDependentTypeNames() const
{
	// Texture and shaders can affect materials which can affect Skinned Assets once they are visible.
	// Adding these dependencies can reduces the actual number of render state update we need to do in a frame
	static FName DependentTypeNames[] = 
	{
		FTextureCompilingManager::GetStaticAssetTypeName(), 
		FShaderCompilingManager::GetStaticAssetTypeName() 
	};
	return TArrayView<FName>(DependentTypeNames);
}

int32 FSkinnedAssetCompilingManager::GetNumRemainingAssets() const
{
	return GetNumRemainingJobs();
}

EQueuedWorkPriority FSkinnedAssetCompilingManager::GetBasePriority(USkinnedAsset* InSkinnedAsset) const
{
	return EQueuedWorkPriority::Low;
}

FQueuedThreadPool* FSkinnedAssetCompilingManager::GetThreadPool() const
{
	static FQueuedThreadPoolDynamicWrapper* GSkinnedAssetThreadPool = nullptr;
	if (GSkinnedAssetThreadPool == nullptr && FAssetCompilingManager::Get().GetThreadPool() != nullptr)
	{
		// For now, skinned assets have almost no high-level awareness of their async behavior.
		// Let them build first to avoid game-thread stalls as much as possible.
		TFunction<EQueuedWorkPriority(EQueuedWorkPriority)> PriorityMapper = [](EQueuedWorkPriority) { return EQueuedWorkPriority::Highest; };

		// Skinned assets will be scheduled on the asset thread pool, where concurrency limits might by dynamically adjusted depending on memory constraints.
		GSkinnedAssetThreadPool = new FQueuedThreadPoolDynamicWrapper(FAssetCompilingManager::Get().GetThreadPool(), -1, PriorityMapper);

		AsyncCompilationHelpers::BindThreadPoolToCVar(
			GSkinnedAssetThreadPool,
			CVarAsyncSkinnedAssetStandard.AsyncCompilation,
			CVarAsyncSkinnedAssetStandard.AsyncCompilationResume,
			CVarAsyncSkinnedAssetStandard.AsyncCompilationMaxConcurrency
		);
	}

	return GSkinnedAssetThreadPool;
}

void FSkinnedAssetCompilingManager::Shutdown()
{
	bHasShutdown = true;
	if (GetNumRemainingJobs())
	{
		check(IsInGameThread());
		TRACE_CPUPROFILER_EVENT_SCOPE(FSkinnedAssetCompilingManager::Shutdown)

		if (GetNumRemainingJobs())
		{
			TArray<USkinnedAsset*> PendingSkinnedAssets;
			PendingSkinnedAssets.Reserve(GetNumRemainingJobs());

			for (TWeakObjectPtr<USkinnedAsset>& WeakSkinnedAsset : RegisteredSkinnedAsset)
			{
				if (WeakSkinnedAsset.IsValid())
				{
					USkinnedAsset* SkinnedAsset = WeakSkinnedAsset.Get();
					if (!SkinnedAsset->IsAsyncTaskComplete())
					{
						if (SkinnedAsset->AsyncTask->Cancel())
						{
							SkinnedAsset->AsyncTask.Reset();
						}
					}

					if (SkinnedAsset->AsyncTask)
					{
						PendingSkinnedAssets.Add(SkinnedAsset);
					}
				}
			}

			FinishCompilation(PendingSkinnedAssets);
		}
	}

	FCoreUObjectDelegates::PostReachabilityAnalysis.Remove(PostReachabilityAnalysisHandle);
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(PreGarbageCollectHandle);
}

bool FSkinnedAssetCompilingManager::IsAsyncCompilationEnabled() const
{
	if (bHasShutdown)
	{
		return false;
	}

	return CVarAsyncSkinnedAssetStandard.AsyncCompilation.GetValueOnAnyThread() != 0;
}

TRACE_DECLARE_INT_COUNTER(QueuedSkinnedAssetCompilation, TEXT("AsyncCompilation/QueuedSkinnedAsset"));
void FSkinnedAssetCompilingManager::UpdateCompilationNotification()
{
	TRACE_COUNTER_SET(QueuedSkinnedAssetCompilation, GetNumRemainingJobs());
	Notification->Update(GetNumRemainingJobs());
}

void FSkinnedAssetCompilingManager::PostCompilation(TArrayView<USkinnedAsset* const> InSkinnedAssets)
{
	if (InSkinnedAssets.Num())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(OnAssetPostCompileEvent);

		TArray<FAssetCompileData> AssetsData;
		AssetsData.Reserve(InSkinnedAssets.Num());

		for (USkinnedAsset* SkinnedAsset : InSkinnedAssets)
		{
			AssetsData.Emplace(SkinnedAsset);
		}

		FAssetCompilingManager::Get().OnAssetPostCompileEvent().Broadcast(AssetsData);
	}
}

void FSkinnedAssetCompilingManager::PostCompilation(USkinnedAsset* SkinnedAsset)
{
	using namespace SkinnedAssetCompilingManagerImpl;
	
	// If AsyncTask is null here, the task got canceled so we don't need to do anything
	if (SkinnedAsset->AsyncTask)
	{
		check(IsInGameThread());
		TRACE_CPUPROFILER_EVENT_SCOPE(FSkinnedAssetCompilingManager::PostCompilation);

		UE_LOG(LogSkeletalMesh, Verbose, TEXT("Refreshing skinned asset %s because it is ready"), *SkinnedAsset->GetName()); // TODO: change to LogSkinnedAsset

		FObjectCacheContextScope ObjectCacheScope;

		// The scope is important here to destroy the FSkinnedAssetAsyncBuildScope before broadcasting events
		{
			// Acquire the async task locally to protect against re-entrance
			TUniquePtr<FSkinnedAssetAsyncBuildTask> LocalAsyncTask = MoveTemp(SkinnedAsset->AsyncTask);
			LocalAsyncTask->EnsureCompletion();

			FSkinnedAssetAsyncBuildScope AsyncBuildScope(SkinnedAsset);

			if (LocalAsyncTask->GetTask().PostLoadContext.IsSet())
			{
				SkinnedAsset->FinishPostLoadInternal(*LocalAsyncTask->GetTask().PostLoadContext);

				LocalAsyncTask->GetTask().PostLoadContext.Reset();
			}

			if (LocalAsyncTask->GetTask().BuildContext.IsSet())
			{
				SkinnedAsset->FinishBuildInternal(*LocalAsyncTask->GetTask().BuildContext);

				LocalAsyncTask->GetTask().BuildContext.Reset();
			}

			if (LocalAsyncTask->GetTask().AsyncTaskContext.IsSet())
			{
				SkinnedAsset->FinishAsyncTaskInternal(*LocalAsyncTask->GetTask().AsyncTaskContext);

				LocalAsyncTask->GetTask().AsyncTaskContext.Reset();
			}
		}

		// Calling this delegate during app exit might be quite dangerous and lead to crash
		// if the content browser wants to refresh a thumbnail it might try to load a package
		// which will then fail due to various reasons related to the editor shutting down.
		// Triggering this callback while garbage collecting can also result in listeners trying to look up objects
		if (!GExitPurge && !IsGarbageCollecting())
		{
			// Generate an empty property changed event, to force the asset registry tag
			// to be refreshed now that RenderData is available.
			FPropertyChangedEvent EmptyPropertyChangedEvent(nullptr);
			FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(SkinnedAsset, EmptyPropertyChangedEvent);
		}
	}
}

bool FSkinnedAssetCompilingManager::IsAsyncCompilationAllowed(USkinnedAsset* SkinnedAsset) const
{
	return IsAsyncCompilationEnabled();
}

FSkinnedAssetCompilingManager& FSkinnedAssetCompilingManager::Get()
{
	static FSkinnedAssetCompilingManager Singleton;
	return Singleton;
}

int32 FSkinnedAssetCompilingManager::GetNumRemainingJobs() const
{
	return RegisteredSkinnedAsset.Num();
}

void FSkinnedAssetCompilingManager::AddSkeletalMeshes(TArrayView<USkeletalMesh* const> InSkeletalMeshes)
{
	TArray<USkinnedAsset*> SkinnedAssets;
	SkinnedAssets.Reserve(InSkeletalMeshes.Num());
	for (USkeletalMesh* SkMesh : InSkeletalMeshes)
	{
		SkinnedAssets.Add(SkMesh);
	}

	AddSkinnedAssets(SkinnedAssets);
}

void FSkinnedAssetCompilingManager::AddSkinnedAssets(TArrayView<USkinnedAsset* const> InSkinnedAssets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSkinnedAssetCompilingManager::AddSkinnedAssets)
	check(IsInGameThread());

	// Wait until we gather enough mesh to process
	// to amortize the cost of scanning components
	//ProcessSkinnedAssets(32 /* MinBatchSize */);

	for (USkinnedAsset* SkinnedAsset : InSkinnedAssets)
	{
		check(SkinnedAsset->AsyncTask != nullptr);
		RegisteredSkinnedAsset.Emplace(SkinnedAsset);
	}

	UpdateCompilationNotification();
}

void FSkinnedAssetCompilingManager::FinishCompilation(TArrayView<USkinnedAsset* const> InSkinnedAssets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSkinnedAssetCompilingManager::FinishCompilation);

	check(IsInGameThread());

	TArray<USkinnedAsset*> PendingSkinnedAssets;
	PendingSkinnedAssets.Reserve(InSkinnedAssets.Num());

	for (USkinnedAsset* SkinnedAsset : InSkinnedAssets)
	{
		if (RegisteredSkinnedAsset.Contains(SkinnedAsset))
		{
			PendingSkinnedAssets.Emplace(SkinnedAsset);
		}
	}

	if (PendingSkinnedAssets.Num())
	{
		class FCompilableSkinnedAsset : public AsyncCompilationHelpers::TCompilableAsyncTask<FSkinnedAssetAsyncBuildTask>
		{
		public:
			FCompilableSkinnedAsset(USkinnedAsset* InSkinnedAsset)
				: SkinnedAsset(InSkinnedAsset)
			{
			}

			FSkinnedAssetAsyncBuildTask* GetAsyncTask() override
			{
				return SkinnedAsset->AsyncTask.Get();
			}

			TStrongObjectPtr<USkinnedAsset> SkinnedAsset;
			FName GetName() override { return SkinnedAsset->GetFName(); }
		};

		TArray<FCompilableSkinnedAsset> CompilableSkinnedAsset(PendingSkinnedAssets);

		FObjectCacheContextScope ObjectCacheScope;
		AsyncCompilationHelpers::FinishCompilation(
			[&CompilableSkinnedAsset](int32 Index) -> AsyncCompilationHelpers::ICompilable& { return CompilableSkinnedAsset[Index]; },
			CompilableSkinnedAsset.Num(),
			LOCTEXT("SkinnedAssets", "Skinned Assets"),
			LogSkeletalMesh, // TODO: change to LogSkinnedAsset
			[this](AsyncCompilationHelpers::ICompilable* Object)
			{
				USkinnedAsset* SkinnedAsset = static_cast<FCompilableSkinnedAsset*>(Object)->SkinnedAsset.Get();
				PostCompilation(SkinnedAsset);
				RegisteredSkinnedAsset.Remove(SkinnedAsset);
			}
		);

		PostCompilation(PendingSkinnedAssets);

		UpdateCompilationNotification();
	}
}

void FSkinnedAssetCompilingManager::FinishCompilationsForGame()
{
	
}

void FSkinnedAssetCompilingManager::FinishAllCompilation()
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FSkinnedAssetCompilingManager::FinishAllCompilation)

	if (GetNumRemainingJobs())
	{
		TArray<USkinnedAsset*> PendingSkinnedAssets;
		PendingSkinnedAssets.Reserve(GetNumRemainingJobs());

		for (TWeakObjectPtr<USkinnedAsset>& SkinnedAsset : RegisteredSkinnedAsset)
		{
			if (SkinnedAsset.IsValid())
			{
				PendingSkinnedAssets.Add(SkinnedAsset.Get());
			}
		}

		FinishCompilation(PendingSkinnedAssets);
	}
}

void FSkinnedAssetCompilingManager::FinishCompilationForObjects(TArrayView<UObject* const> InObjects)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSkinnedAssetCompilingManager::FinishCompilationForObjects);

	TSet<USkinnedAsset*> SkinnedAssets;
	for (UObject* Object : InObjects)
	{
		if (USkinnedAsset* SkinnedAsset = Cast<USkinnedAsset>(Object))
		{
			SkinnedAssets.Add(SkinnedAsset);
		}
		else if (USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(Object))
		{
			if (SkinnedMeshComponent->GetSkinnedAsset())
			{
				SkinnedAssets.Add(SkinnedMeshComponent->GetSkinnedAsset());
			}
		}
	}

	if (SkinnedAssets.Num())
	{
		FinishCompilation(SkinnedAssets.Array());
	}
}

void FSkinnedAssetCompilingManager::Reschedule()
{

}

void FSkinnedAssetCompilingManager::ProcessSkinnedAssets(bool bLimitExecutionTime, int32 MinBatchSize)
{
	using namespace SkinnedAssetCompilingManagerImpl;
	TRACE_CPUPROFILER_EVENT_SCOPE(FSkinnedAssetCompilingManager::ProcessSkinnedAssets);
	const int32 NumRemainingMeshes = GetNumRemainingJobs();
	// Spread out the load over multiple frames but if too many meshes, convergence is more important than frame time
	const int32 MaxMeshUpdatesPerFrame = bLimitExecutionTime ? FMath::Max(64, NumRemainingMeshes / 10) : INT32_MAX;

	FObjectCacheContextScope ObjectCacheScope;
	if (NumRemainingMeshes && NumRemainingMeshes >= MinBatchSize)
	{
		TSet<USkinnedAsset*> SkinnedAssetsToProcess;
		for (TWeakObjectPtr<USkinnedAsset>& SkinnedAsset : RegisteredSkinnedAsset)
		{
			if (SkinnedAsset.IsValid())
			{
				SkinnedAssetsToProcess.Add(SkinnedAsset.Get());
			}
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ProcessFinishedSkinnedAssets);

			const double TickStartTime = FPlatformTime::Seconds();

			TSet<TWeakObjectPtr<USkinnedAsset>> SkinnedAssetsToPostpone;
			TArray<USkinnedAsset*> ProcessedSkinnedAssets;
			if (SkinnedAssetsToProcess.Num())
			{
				for (USkinnedAsset* SkinnedAsset : SkinnedAssetsToProcess)
				{
					const bool bHasMeshUpdateLeft = ProcessedSkinnedAssets.Num() <= MaxMeshUpdatesPerFrame;
					if (bHasMeshUpdateLeft && SkinnedAsset->IsAsyncTaskComplete())
					{
						PostCompilation(SkinnedAsset);
						ProcessedSkinnedAssets.Add(SkinnedAsset);
					}
					else
					{
						SkinnedAssetsToPostpone.Emplace(SkinnedAsset);
					}
				}
			}

			RegisteredSkinnedAsset = MoveTemp(SkinnedAssetsToPostpone);

			PostCompilation(ProcessedSkinnedAssets);
		}
	}
}

void FSkinnedAssetCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	FObjectCacheContextScope ObjectCacheScope;
	FinishCompilationsForGame();

	Reschedule();

	ProcessSkinnedAssets(bLimitExecutionTime);

	UpdateCompilationNotification();
}

void FSkinnedAssetCompilingManager::OnPostReachabilityAnalysis()
{
	if (GetNumRemainingJobs())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSkinnedAssetCompilingManager::CancelUnreachableMeshes);

		TArray<USkinnedAsset*> PendingSkinnedMeshes;
		PendingSkinnedMeshes.Reserve(GetNumRemainingJobs());

		for (auto Iterator = RegisteredSkinnedAsset.CreateIterator(); Iterator; ++Iterator)
		{
			USkinnedAsset* SkinnedMesh = Iterator->GetEvenIfUnreachable();
			if (SkinnedMesh && SkinnedMesh->IsUnreachable())
			{
				UE_LOG(LogSkeletalMesh, Verbose, TEXT("Cancelling skinned mesh %s async compilation because it's being garbage collected"), *SkinnedMesh->GetName());

				if (SkinnedMesh->TryCancelAsyncTasks())
				{
					Iterator.RemoveCurrent();
				}
				else
				{
					PendingSkinnedMeshes.Add(SkinnedMesh);
				}
			}
		}

		FinishCompilation(PendingSkinnedMeshes);
	}
}

void FSkinnedAssetCompilingManager::OnPreGarbageCollect()
{
	FinishAllCompilation();
}

#endif // #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE
