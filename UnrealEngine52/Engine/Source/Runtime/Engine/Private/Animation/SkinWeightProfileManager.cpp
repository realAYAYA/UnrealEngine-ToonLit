// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/SkinWeightProfileManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "Rendering/SkeletalMeshRenderData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinWeightProfileManager)

DEFINE_LOG_CATEGORY_STATIC(LogSkinWeightProfileManager, Warning, Display);

#define PROFILE_SKIN_WEIGHT_MANAGER (!UE_BUILD_SHIPPING)

static int32 GAllowCPU = 1;
static FAutoConsoleVariableRef CVarAllowCPU (
	TEXT("SkinWeightProfileManager.AllowCPU"),
	GAllowCPU,
	TEXT("Whether or not to allow cpu buffer generation"),
	ECVF_Cheat
);

FSkinWeightProfileManager::FSkinWeightProfileManager(UWorld* InWorld) : LastGamethreadProfileIndex(INDEX_NONE), WeakWorld(nullptr)
{
	if (InWorld)
	{
		TickFunction.TickGroup = ETickingGroup::TG_PrePhysics;
		TickFunction.EndTickGroup = ETickingGroup::TG_PostUpdateWork;
		TickFunction.bCanEverTick = true;
		TickFunction.bStartWithTickEnabled = true;
		TickFunction.bTickEvenWhenPaused = false;
		TickFunction.Owner = this;
		TickFunction.RegisterTickFunction(InWorld->PersistentLevel);

		WeakWorld = InWorld;
	}	
}

TMap<UWorld*, FSkinWeightProfileManager*> FSkinWeightProfileManager::WorldManagers;

void FSkinWeightProfileManager::OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	check(WorldManagers.Find(World) == nullptr);

	FSkinWeightProfileManager* NewWorldMan = new FSkinWeightProfileManager(World);
	WorldManagers.Add(World) = NewWorldMan;
}

void FSkinWeightProfileManager::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (FSkinWeightProfileManager* *WorldMan = WorldManagers.Find(World))
	{
		delete (*WorldMan);
		WorldManagers.Remove(World);
	}
}

void FSkinWeightProfileManager::OnPreWorldFinishDestroy(UWorld* World)
{
	if (FSkinWeightProfileManager * *WorldMan = WorldManagers.Find(World))
	{
		delete (*WorldMan);
		WorldManagers.Remove(World);
	}
}

void FSkinWeightProfileManager::OnWorldBeginTearDown(UWorld* World)
{
	if (FSkinWeightProfileManager * *WorldMan = WorldManagers.Find(World))
	{
		delete (*WorldMan);
		WorldManagers.Remove(World);
	}
}

FSkinWeightProfileManager* FSkinWeightProfileManager::Get(UWorld* World)
{
	if (FSkinWeightProfileManager * *WorldMan = WorldManagers.Find(World))
	{
		return *WorldMan;
	}

	return nullptr;
}

void FSkinWeightProfileManager::OnStartup()
{
	FWorldDelegates::OnPreWorldInitialization.AddStatic(&OnWorldInit);
	FWorldDelegates::OnPostWorldCleanup.AddStatic(&OnWorldCleanup);
	FWorldDelegates::OnPreWorldFinishDestroy.AddStatic(&OnPreWorldFinishDestroy);
	FWorldDelegates::OnWorldBeginTearDown.AddStatic(&OnWorldBeginTearDown);
}

void FSkinWeightProfileManager::OnShutdown()
{
	for (TPair<UWorld*, FSkinWeightProfileManager*>& Pair : WorldManagers)
	{
		if (Pair.Value)
		{
			delete Pair.Value;
		}
	}
	WorldManagers.Empty();
}

void FSkinWeightProfileManager::RequestSkinWeightProfile(FName InProfileName, USkinnedAsset* SkinnedAsset, UObject* Requester, FRequestFinished& Callback, int32 LODIndex /*= INDEX_NONE*/)
{
	// Make sure we have an actual skeletal mesh
	if (USkeletalMesh* const Mesh = Cast<USkeletalMesh>(SkinnedAsset))
	{
		// Setup a request structure
		FSetProfileRequest ProfileRequest;
		ProfileRequest.ProfileName = InProfileName;
		ProfileRequest.Callback = Callback;
		ProfileRequest.WeakSkeletalMesh = Mesh;
		ProfileRequest.IdentifyingObject = Requester;

		UE_LOG(LogSkinWeightProfileManager, Display, TEXT("RequestSkinWeightProfile [%s | %s]"), *Mesh->GetName(), *Requester->GetName());

		if (LODIndex == INDEX_NONE)
		{
			const int32 NumLODS = Mesh->GetLODNum();
			for (int32 index = 0; index < NumLODS; ++index)
			{
				ProfileRequest.LODIndices.Add(index);
			}
		}
		else
		{
			ProfileRequest.LODIndices.Add(LODIndex);
		}
		
		// Add the profile request
		const int32 Index = PendingSetProfileRequests.Add(ProfileRequest);

		// Check whether or not we might have already setup a readback for this skeletal mesh, and add a request to it
		int32& NumRequestsForMesh = PendingMeshes.FindOrAdd(Mesh, 0);
		++NumRequestsForMesh;

		TickFunction.SetTickFunctionEnable(true);
	}
}

void FSkinWeightProfileManager::CancelSkinWeightProfileRequest(UObject* Requester)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SkinWeightProfileManager_CancelSkinWeightProfileRequest);

	for (FSetProfileRequest& Request : PendingSetProfileRequests)
	{
		if (Request.IdentifyingObject == Requester || Request.WeakSkeletalMesh == Requester)
		{
			CanceledRequest.Add(Request);
			UE_LOG(LogSkinWeightProfileManager, Display, TEXT("Cancel [%s]"), *Requester->GetName());			
		}
	}
}

void FSkinWeightProfileManager::DoTick(float DeltaTime, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{	
#if PROFILE_SKIN_WEIGHT_MANAGER 
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(SkinWeightProfileManagerGT);	
	CSV_CUSTOM_STAT_GLOBAL(SkinWeightProfileManager_PendingMeshes, PendingMeshes.Num(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_GLOBAL(SkinWeightProfileManager_PendingRequests, PendingSetProfileRequests.Num(), ECsvCustomStatOp::Set);
#endif // PROFILE_SKIN_WEIGHT_MANAGER 

	QUICK_SCOPE_CYCLE_COUNTER(STAT_SkinWeightProfileManager_Tick);

	LastGamethreadProfileIndex = INDEX_NONE;

	if (PendingSetProfileRequests.Num())
	{
		if (AsyncTask.IsValid())
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(AsyncTask, ENamedThreads::GameThread);
			AsyncTask.SafeRelease();
		}

		for (int32 RequestIndex = 0; RequestIndex < PendingSetProfileRequests.Num(); ++RequestIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_SkinWeightProfileManager_TickMesh);

			const FSetProfileRequest& Request = PendingSetProfileRequests[RequestIndex];
			
			// Remove if canceled or objects have gone stale 
			bool bRemove = Request.IdentifyingObject.IsStale() || Request.WeakSkeletalMesh.IsStale() ||
				CanceledRequest.ContainsByPredicate([Request](FSetProfileRequest& B)
			{
				return Request.IdentifyingObject == B.IdentifyingObject && Request.WeakSkeletalMesh == B.WeakSkeletalMesh && Request.ProfileName == B.ProfileName;
			});

			const USkeletalMesh* SkeletalMesh = Request.WeakSkeletalMesh.Get();
			FSkeletalMeshRenderData* RenderData = SkeletalMesh ? SkeletalMesh->GetResourceForRendering() : nullptr;
			// Or if skeletal mesh / render data is invalid
			bRemove = bRemove ? true : (RenderData == nullptr);
				
			if (!bRemove)
			{	
				bool bAllBuffersReady = true;
				for (const int32 LODIndex : Request.LODIndices)
				{
					if (!RenderData->LODRenderData[LODIndex].SkinWeightProfilesData.ContainsOverrideBuffer(Request.ProfileName))
					{
						bAllBuffersReady = false;
						break;
					}
				}

				if (bAllBuffersReady)
				{
					if (Request.IdentifyingObject.IsValid())
					{
						UE_LOG(LogSkinWeightProfileManager, Display, TEXT("Callback [%s | %s]"), *SkeletalMesh->GetName(), *Request.IdentifyingObject->GetName());
						Request.Callback(Request.WeakSkeletalMesh, Request.ProfileName);
						bRemove = true;
					}
				}
			}
			
			if (bRemove)
			{
				CleanupRequest(Request);
				CanceledRequest.Remove(Request);
				PendingSetProfileRequests.RemoveAtSwap(RequestIndex);
				--RequestIndex;
			}
		}

		// This is the last index that we will process asynchronously, while the task is running we do not remove request from the array so access should be safe
		LastGamethreadProfileIndex = PendingSetProfileRequests.Num() - 1;

		if (MyCompletionGraphEvent)
		{
			AsyncTask = TGraphTask<FSkinWeightProfileManagerAsyncTask>::CreateTask(nullptr, CurrentThread).ConstructAndDispatchWhenReady(this);
			TickFunction.GetCompletionHandle()->DontCompleteUntil(AsyncTask);
		}
	}
	else
	{
		TickFunction.SetTickFunctionEnable(false);
	}
}

void FSkinWeightProfileManager::Tick(float DeltaTime)
{
	DoTick(DeltaTime, ENamedThreads::GameThread, nullptr);
}

bool FSkinWeightProfileManager::IsTickable() const
{
	return WeakWorld.IsExplicitlyNull();
}

TStatId FSkinWeightProfileManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSkinWeightProfileManager, STATGROUP_Tickables);
}

void FSkinWeightProfileManager::CleanupRequest(const FSetProfileRequest& Request)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SkinWeightProfileManager_CleanupRequest);
	// Remove request for its pending skeletal mesh
	if (int32* RequestsPtr = PendingMeshes.Find(Request.WeakSkeletalMesh))
	{
		int32& NumPendingRequests = *RequestsPtr;
		--NumPendingRequests;

		// In case all requests have finished clean up the readback data and remove skeletal mesh as pending
		if (NumPendingRequests == 0)
		{
			PendingMeshes.Remove(Request.WeakSkeletalMesh);

			if (USkeletalMesh * SkeletalMesh = Request.WeakSkeletalMesh.Get())
			{
				if (FSkeletalMeshRenderData * RenderData = SkeletalMesh->GetResourceForRendering())
				{
					for (FSkeletalMeshLODRenderData& LODRenderData : RenderData->LODRenderData)
					{
						FSkinWeightProfilesData& SkinweightData = LODRenderData.SkinWeightProfilesData;
						SkinweightData.ResetGPUReadback();
					}
				}
			}
		}
	}
	
}

bool FSkinWeightProfileManager::IsTickableWhenPaused() const
{
	return false;
}

bool FSkinWeightProfileManager::IsTickableInEditor() const
{
	return true;
}

void FSkinWeightProfileManagerTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Owner->DoTick(DeltaTime, CurrentThread, MyCompletionGraphEvent);
}

FString FSkinWeightProfileManagerTickFunction::DiagnosticMessage()
{
	return TEXT("FSkinWeightProfileManagerTickFunction::Tick");
}

FName FSkinWeightProfileManagerTickFunction::DiagnosticContext(bool bDetailed)
{
	return FName("FSkinWeightProfileManager");
}

void FSkinWeightProfileManagerAsyncTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
#if PROFILE_SKIN_WEIGHT_MANAGER 
	CSV_SCOPED_TIMING_STAT_GLOBAL(SkinWeightProfileManagerAsync);
#endif // PROFILE_SKIN_WEIGHT_MANAGER 
	
	const TArray<FSetProfileRequest>& OwnerRequests = Owner->PendingSetProfileRequests;

	checkf(Owner->LastGamethreadProfileIndex < OwnerRequests.Num(), TEXT("Invalid state"));

	for (int32 RequestIndex = 0; RequestIndex <= Owner->LastGamethreadProfileIndex; ++RequestIndex)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_SkinWeightProfileManager_TickMeshAsync);

		const FSetProfileRequest& Request = OwnerRequests[RequestIndex];
		if (const USkeletalMesh* SkeletalMesh = Request.WeakSkeletalMesh.Get())
		{
			if (FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering())
			{
				for (const int32 LODIndex : Request.LODIndices)
				{
					FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];
					FSkinWeightProfilesData& SkinweightData = LODRenderData.SkinWeightProfilesData;
					
					if (!!GAllowCPU && LODRenderData.SkinWeightVertexBuffer.GetNeedsCPUAccess())
					{
						SkinweightData.InitialiseProfileBuffer(Request.ProfileName);
					}
					else
					{
						if (SkinweightData.IsPendingReadback())
						{
							SkinweightData.EnqueueGPUReadback();
						}
						else if (SkinweightData.IsGPUReadbackFinished())
						{
							if (SkinweightData.IsGPUReadbackFinished() && !SkinweightData.IsDataReadbackPending())
							{						
								SkinweightData.EnqueueDataReadback();
							}
							else if (SkinweightData.IsDataReadbackFinished())
							{						
								SkinweightData.InitialiseProfileBuffer(Request.ProfileName);
							}
						}
					}
				}
			}
		}
	}
}

