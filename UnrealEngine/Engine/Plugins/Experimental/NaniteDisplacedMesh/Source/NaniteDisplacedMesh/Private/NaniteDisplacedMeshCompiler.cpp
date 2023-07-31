// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDisplacedMeshCompiler.h"

#if WITH_EDITOR

#include "Algo/NoneOf.h"
#include "AssetCompilingManager.h"
#include "ObjectCacheContext.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "Misc/Optional.h"
#include "EngineModule.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/IQueuedWork.h"
#include "StaticMeshCompiler.h"
#include "NaniteDisplacedMeshLog.h"
#include "NaniteDisplacedMeshComponent.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Components/PrimitiveComponent.h"

#define LOCTEXT_NAMESPACE "NaniteDisplacedMeshCompiler"

static AsyncCompilationHelpers::FAsyncCompilationStandardCVars CVarAsyncNaniteDisplacedMeshStandard(
	TEXT("NaniteDisplacedMesh"),
	TEXT("nanite displaced meshes"),
	FConsoleCommandDelegate::CreateLambda(
		[]()
		{
			FNaniteDisplacedMeshCompilingManager::Get().FinishAllCompilation();
		}
));

namespace NaniteDisplacedMeshCompilingManagerImpl
{
	static void EnsureInitializedCVars()
	{
		static bool bIsInitialized = false;

		if (!bIsInitialized)
		{
			bIsInitialized = true;

			AsyncCompilationHelpers::EnsureInitializedCVars(
				TEXT("NaniteDisplacedMesh"),
				CVarAsyncNaniteDisplacedMeshStandard.AsyncCompilation,
				CVarAsyncNaniteDisplacedMeshStandard.AsyncCompilationMaxConcurrency);
		}
	}
}

FNaniteDisplacedMeshCompilingManager::FNaniteDisplacedMeshCompilingManager()
	: Notification(GetAssetNameFormat())
{
	NaniteDisplacedMeshCompilingManagerImpl::EnsureInitializedCVars();

	PostReachabilityAnalysisHandle = FCoreUObjectDelegates::PostReachabilityAnalysis.AddRaw(this, &FNaniteDisplacedMeshCompilingManager::OnPostReachabilityAnalysis);
}

void FNaniteDisplacedMeshCompilingManager::OnPostReachabilityAnalysis()
{
	if (GetNumRemainingAssets())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FNaniteDisplacedMeshCompilingManager::CancelUnreachableMeshes);

		TArray<UNaniteDisplacedMesh*> PendingNaniteDisplacedMeshes;
		PendingNaniteDisplacedMeshes.Reserve(GetNumRemainingAssets());

		for (auto Iterator = RegisteredNaniteDisplacedMesh.CreateIterator(); Iterator; ++Iterator)
		{
			UNaniteDisplacedMesh* NaniteDisplacedMesh = Iterator->GetEvenIfUnreachable();
			if (NaniteDisplacedMesh && NaniteDisplacedMesh->IsUnreachable())
			{
				UE_LOG(LogNaniteDisplacedMesh, Verbose, TEXT("Cancelling nanite displaced mesh %s compilation because it's being garbage collected"), *NaniteDisplacedMesh->GetName());

				if (NaniteDisplacedMesh->TryCancelAsyncTasks())
				{
					Iterator.RemoveCurrent();
				}
				else
				{
					PendingNaniteDisplacedMeshes.Add(NaniteDisplacedMesh);
				}
			}
		}

		FinishCompilation(PendingNaniteDisplacedMeshes);
	}
}

FName FNaniteDisplacedMeshCompilingManager::GetAssetTypeName() const
{
	return TEXT("UE-NaniteDisplacedMesh");
}

FTextFormat FNaniteDisplacedMeshCompilingManager::GetAssetNameFormat() const
{
	return LOCTEXT("NaniteDisplacedMeshNameFormat", "{0}|plural(one=Nanite Displaced Mesh,other=Nanite Displaced Meshes)");
}

TArrayView<FName> FNaniteDisplacedMeshCompilingManager::GetDependentTypeNames() const
{
	static FName DependentTypeNames[] =	
	{ 
		// NaniteDisplacedMesh can wait on StaticMesh to finish their own compilation before compiling themselves
		// so they need to be processed before us. This is especially important when FinishAllCompilation is issued
		// so that we know once we're called that all static mesh have finished compiling.
		FStaticMeshCompilingManager::GetStaticAssetTypeName() 
	};
	return TArrayView<FName>(DependentTypeNames);
}

EQueuedWorkPriority FNaniteDisplacedMeshCompilingManager::GetBasePriority(UNaniteDisplacedMesh* InNaniteDisplacedMesh) const
{
	return EQueuedWorkPriority::Low;
}

FQueuedThreadPool* FNaniteDisplacedMeshCompilingManager::GetThreadPool() const
{
	static FQueuedThreadPoolDynamicWrapper* GNaniteDisplacedMeshThreadPool = nullptr;
	if (GNaniteDisplacedMeshThreadPool == nullptr && FAssetCompilingManager::Get().GetThreadPool() != nullptr)
	{
		// Nanite displaced meshes will be scheduled on the asset thread pool, where concurrency limits might by dynamically adjusted depending on memory constraints.
		GNaniteDisplacedMeshThreadPool = new FQueuedThreadPoolDynamicWrapper(FAssetCompilingManager::Get().GetThreadPool(), -1, [](EQueuedWorkPriority) { return EQueuedWorkPriority::Low; });

		AsyncCompilationHelpers::BindThreadPoolToCVar(
			GNaniteDisplacedMeshThreadPool,
			CVarAsyncNaniteDisplacedMeshStandard.AsyncCompilation,
			CVarAsyncNaniteDisplacedMeshStandard.AsyncCompilationResume,
			CVarAsyncNaniteDisplacedMeshStandard.AsyncCompilationMaxConcurrency
		);
	}

	return GNaniteDisplacedMeshThreadPool;
}

void FNaniteDisplacedMeshCompilingManager::Shutdown()
{
	bHasShutdown = true;
	if (GetNumRemainingAssets())
	{
		check(IsInGameThread());
		TRACE_CPUPROFILER_EVENT_SCOPE(FNaniteDisplacedMeshCompilingManager::Shutdown);

		TArray<UNaniteDisplacedMesh*> PendingNaniteDisplacedMeshes;
		PendingNaniteDisplacedMeshes.Reserve(GetNumRemainingAssets());

		for (TWeakObjectPtr<UNaniteDisplacedMesh>& WeakNaniteDisplacedMesh : RegisteredNaniteDisplacedMesh)
		{
			if (WeakNaniteDisplacedMesh.IsValid())
			{
				UNaniteDisplacedMesh* NaniteDisplacedMesh = WeakNaniteDisplacedMesh.Get();
				if (!NaniteDisplacedMesh->TryCancelAsyncTasks())
				{
					PendingNaniteDisplacedMeshes.Add(NaniteDisplacedMesh);
				}
			}
		}

		FinishCompilation(PendingNaniteDisplacedMeshes);
	}

	FCoreUObjectDelegates::PostReachabilityAnalysis.Remove(PostReachabilityAnalysisHandle);
}

void FNaniteDisplacedMeshCompilingManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(GCReferedNaniteDisplacedMesh);
}

FString FNaniteDisplacedMeshCompilingManager::GetReferencerName() const
{
	return FString(TEXT("FNaniteDisplacedMeshCompilingManager"));
}

TRACE_DECLARE_INT_COUNTER(QueuedNaniteDisplacedMeshCompilation, TEXT("AsyncCompilation/QueuedNaniteDisplacedMesh"));
void FNaniteDisplacedMeshCompilingManager::UpdateCompilationNotification()
{
	TRACE_COUNTER_SET(QueuedNaniteDisplacedMeshCompilation, GetNumRemainingAssets());
	Notification.Update(GetNumRemainingAssets());
}

void FNaniteDisplacedMeshCompilingManager::PostCompilation(TArrayView<UNaniteDisplacedMesh* const> InNaniteDisplacedMeshes)
{
	if (InNaniteDisplacedMeshes.Num())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(OnAssetPostCompileEvent);

		TArray<FAssetCompileData> AssetsData;
		AssetsData.Reserve(InNaniteDisplacedMeshes.Num());

		for (UNaniteDisplacedMesh* NaniteDisplacedMesh : InNaniteDisplacedMeshes)
		{
			// Do not broadcast an event for unreachable objects
			if (!NaniteDisplacedMesh->IsUnreachable())
			{
				AssetsData.Emplace(NaniteDisplacedMesh);

				if (FApp::CanEverRender())
				{
					NaniteDisplacedMesh->InitResources();
					NaniteDisplacedMesh->NotifyOnRenderingDataChanged();
				}
			}
		}

		if (AssetsData.Num())
		{
			FAssetCompilingManager::Get().OnAssetPostCompileEvent().Broadcast(AssetsData);
		}
	}
}

void FNaniteDisplacedMeshCompilingManager::PostCompilation(UNaniteDisplacedMesh* NaniteDisplacedMesh)
{
	using namespace NaniteDisplacedMeshCompilingManagerImpl;

	// If AsyncTask is null here, the task got canceled so we don't need to do anything
	if (!IsEngineExitRequested())
	{
		check(IsInGameThread());
		TRACE_CPUPROFILER_EVENT_SCOPE(PostCompilation);

		NaniteDisplacedMesh->FinishAsyncTasks();

		// Do not do anything else if the NaniteDisplacedMesh is being garbage collected
		if (NaniteDisplacedMesh->IsUnreachable())
		{
			return;
		}

		NaniteDisplacedMesh->InitResources();

		// Calling this delegate during app exit might be quite dangerous and lead to crash
		// if the content browser wants to refresh a thumbnail it might try to load a package
		// which will then fail due to various reasons related to the editor shutting down.
		// Triggering this callback while garbage collecting can also result in listeners trying to look up objects
		if (!GExitPurge && !IsGarbageCollecting())
		{
			// Generate an empty property changed event, to force the asset registry tag
			// to be refreshed now that RenderData is available.
			FPropertyChangedEvent EmptyPropertyChangedEvent(nullptr);
			FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(NaniteDisplacedMesh, EmptyPropertyChangedEvent);
		}
	}
}

FNaniteDisplacedMeshCompilingManager& FNaniteDisplacedMeshCompilingManager::Get()
{
	static FNaniteDisplacedMeshCompilingManager Singleton;
	return Singleton;
}

int32 FNaniteDisplacedMeshCompilingManager::GetNumRemainingAssets() const
{
	return RegisteredNaniteDisplacedMesh.Num();
}

void FNaniteDisplacedMeshCompilingManager::AddNaniteDisplacedMeshes(TArrayView<UNaniteDisplacedMesh* const> InNaniteDisplacedMeshes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FNaniteDisplacedMeshCompilingManager::AddNaniteDisplacedMeshes);
	check(IsInGameThread());

	for (UNaniteDisplacedMesh* NaniteDisplacedMesh : InNaniteDisplacedMeshes)
	{
		RegisteredNaniteDisplacedMesh.Emplace(NaniteDisplacedMesh);
		if (NaniteDisplacedMesh
			&& NaniteDisplacedMesh->HasAnyFlags(RF_Transient)
			&& !NaniteDisplacedMesh->HasAnyFlags(RF_Standalone))
		{
			/**
			 * Transient generated object shouldn't stall the GC.
			 * So we refer then here to delay when they will be GC if they are compiling.
			 */
			GCReferedNaniteDisplacedMesh.Add(NaniteDisplacedMesh);
		}
	}

	TRACE_COUNTER_SET(QueuedNaniteDisplacedMeshCompilation, GetNumRemainingAssets());
}

void FNaniteDisplacedMeshCompilingManager::FinishCompilation(TArrayView<UNaniteDisplacedMesh* const> InNaniteDisplacedMeshes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FNaniteDisplacedMeshCompilingManager::FinishCompilation);

	// Allow calls from any thread if the meshes are already finished compiling.
	if (Algo::NoneOf(InNaniteDisplacedMeshes, &UNaniteDisplacedMesh::IsCompiling))
	{
		return;
	}

	check(IsInGameThread());

	TArray<UNaniteDisplacedMesh*> PendingNaniteDisplacedMeshes;
	PendingNaniteDisplacedMeshes.Reserve(InNaniteDisplacedMeshes.Num());

	int32 NaniteDisplacedMeshIndex = 0;
	for (UNaniteDisplacedMesh* NaniteDisplacedMesh : InNaniteDisplacedMeshes)
	{
		if (RegisteredNaniteDisplacedMesh.Contains(NaniteDisplacedMesh))
		{
			PendingNaniteDisplacedMeshes.Emplace(NaniteDisplacedMesh);
		}
	}

	if (PendingNaniteDisplacedMeshes.Num())
	{
		class FCompilableNaniteDisplacedMesh : public AsyncCompilationHelpers::ICompilable
		{
		public:
			FCompilableNaniteDisplacedMesh(UNaniteDisplacedMesh* InNaniteDisplacedMesh)
				: NaniteDisplacedMesh(InNaniteDisplacedMesh)
			{
			}

			void Reschedule(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority)
			{
				NaniteDisplacedMesh->Reschedule(InThreadPool, InPriority);
			}

			bool WaitCompletionWithTimeout(float TimeLimitSeconds) override
			{
				// Poll for now but we might want to use events to wait instead at some point
				if (!NaniteDisplacedMesh->IsAsyncTaskComplete())
				{
					FPlatformProcess::Sleep(TimeLimitSeconds);
					return false;
				}

				return true;
			}

			UNaniteDisplacedMesh* NaniteDisplacedMesh;
			FName GetName() override { return NaniteDisplacedMesh->GetOutermost()->GetFName(); }
		};

		TArray<FCompilableNaniteDisplacedMesh> CompilableNaniteDisplacedMeshes(PendingNaniteDisplacedMeshes);
		FObjectCacheContextScope ObjectCacheScope;
		AsyncCompilationHelpers::FinishCompilation(
			[&CompilableNaniteDisplacedMeshes](int32 Index)	-> AsyncCompilationHelpers::ICompilable& { return CompilableNaniteDisplacedMeshes[Index]; },
			CompilableNaniteDisplacedMeshes.Num(),
			LOCTEXT("NaniteDisplacedMeshes", "Nanite Displaced Meshes"),
			LogNaniteDisplacedMesh,
			[this](AsyncCompilationHelpers::ICompilable* Object)
			{
				UNaniteDisplacedMesh* NaniteDisplacedMesh = static_cast<FCompilableNaniteDisplacedMesh*>(Object)->NaniteDisplacedMesh;
				PostCompilation(NaniteDisplacedMesh);
				RegisteredNaniteDisplacedMesh.Remove(NaniteDisplacedMesh);
				GCReferedNaniteDisplacedMesh.Remove(NaniteDisplacedMesh);
			}
		);

		PostCompilation(PendingNaniteDisplacedMeshes);
	}
}

void FNaniteDisplacedMeshCompilingManager::FinishCompilationsForGame()
{
	// Nothing special to do when we PIE for now.
}

void FNaniteDisplacedMeshCompilingManager::FinishAllCompilation()
{
	check(IsInGameThread());
	TRACE_CPUPROFILER_EVENT_SCOPE(FNaniteDisplacedMeshCompilingManager::FinishAllCompilation)

		if (GetNumRemainingAssets())
		{
			TArray<UNaniteDisplacedMesh*> PendingNaniteDisplacedMeshes;
			PendingNaniteDisplacedMeshes.Reserve(GetNumRemainingAssets());

			for (TWeakObjectPtr<UNaniteDisplacedMesh>& NaniteDisplacedMesh : RegisteredNaniteDisplacedMesh)
			{
				if (NaniteDisplacedMesh.IsValid())
				{
					PendingNaniteDisplacedMeshes.Add(NaniteDisplacedMesh.Get());
				}
			}

			FinishCompilation(PendingNaniteDisplacedMeshes);
		}
}

void FNaniteDisplacedMeshCompilingManager::Reschedule()
{
	// TODO Prioritize nanite displaced mesh that are nearest to the viewport
}

void FNaniteDisplacedMeshCompilingManager::ProcessNaniteDisplacedMeshes(bool bLimitExecutionTime, int32 MinBatchSize)
{
	using namespace NaniteDisplacedMeshCompilingManagerImpl;
	TRACE_CPUPROFILER_EVENT_SCOPE(FNaniteDisplacedMeshCompilingManager::ProcessNaniteDisplacedMeshes);
	const int32 NumRemainingMeshes = GetNumRemainingAssets();
	// Spread out the load over multiple frames but if too many meshes, convergence is more important than frame time
	const int32 MaxMeshUpdatesPerFrame = bLimitExecutionTime ? FMath::Max(64, NumRemainingMeshes / 10) : INT32_MAX;

	FObjectCacheContextScope ObjectCacheScope;
	if (NumRemainingMeshes && NumRemainingMeshes >= MinBatchSize)
	{
		TSet<UNaniteDisplacedMesh*> NaniteDisplacedMeshesToProcess;
		for (TWeakObjectPtr<UNaniteDisplacedMesh>& NaniteDisplacedMesh : RegisteredNaniteDisplacedMesh)
		{
			if (NaniteDisplacedMesh.IsValid())
			{
				NaniteDisplacedMeshesToProcess.Add(NaniteDisplacedMesh.Get());
			}
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ProcessFinishedNaniteDisplacedMeshes);

			const double TickStartTime = FPlatformTime::Seconds();

			TSet<TWeakObjectPtr<UNaniteDisplacedMesh>> NaniteDisplacedMeshesToPostpone;
			TArray<UNaniteDisplacedMesh*> ProcessedNaniteDisplacedMeshes;
			if (NaniteDisplacedMeshesToProcess.Num())
			{
				for (UNaniteDisplacedMesh* NaniteDisplacedMesh : NaniteDisplacedMeshesToProcess)
				{
					const bool bHasMeshUpdateLeft = ProcessedNaniteDisplacedMeshes.Num() <= MaxMeshUpdatesPerFrame;
					if (bHasMeshUpdateLeft && NaniteDisplacedMesh->IsAsyncTaskComplete())
					{
						PostCompilation(NaniteDisplacedMesh);
						ProcessedNaniteDisplacedMeshes.Add(NaniteDisplacedMesh);
						GCReferedNaniteDisplacedMesh.Remove(NaniteDisplacedMesh);
					}
					else
					{
						NaniteDisplacedMeshesToPostpone.Emplace(NaniteDisplacedMesh);
					}
				}
			}

			RegisteredNaniteDisplacedMesh = MoveTemp(NaniteDisplacedMeshesToPostpone);

			PostCompilation(ProcessedNaniteDisplacedMeshes);
		}
	}
}

void FNaniteDisplacedMeshCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	FObjectCacheContextScope ObjectCacheScope;
	FinishCompilationsForGame();

	Reschedule();

	ProcessNaniteDisplacedMeshes(bLimitExecutionTime);

	UpdateCompilationNotification();
}

#undef LOCTEXT_NAMESPACE

#endif // #if WITH_EDITOR
