// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "RHIGPUReadback.h"
#include "Tickable.h"
#include "Templates/Function.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectMacros.h"
#include "Engine/World.h"

#include "SkinWeightProfileManager.generated.h"

class FSkinWeightProfileManager;
class UWorld;

typedef TFunction<void(TWeakObjectPtr<USkeletalMesh> WeakMesh, FName ProfileName)> FRequestFinished;

/** Describes a single skin weight profile request */
struct FSetProfileRequest
{
	/** Name of the skin weight profile to be loaded */
	FName ProfileName;
	/** LOD Indices to load the profile for */
	TArray<int32> LODIndices;
	/** Called when the profile request has finished and data is ready (called from GT only) */
	FRequestFinished Callback;

	/** Weak UObject that is responsible for this request */
	TWeakObjectPtr<UObject> IdentifyingObject;
	/** Weak skeletal mesh for which the skin weight profile is to be loaded */
	TWeakObjectPtr<USkeletalMesh> WeakSkeletalMesh;

	friend bool operator==(const FSetProfileRequest& A, const FSetProfileRequest& B)
	{
		return A.ProfileName == B.ProfileName && A.WeakSkeletalMesh == B.WeakSkeletalMesh && A.IdentifyingObject == B.IdentifyingObject;
	}

	friend uint32 GetTypeHash(FSetProfileRequest A)
	{
		return HashCombine(GetTypeHash(A.ProfileName), GetTypeHash(A.WeakSkeletalMesh));
	}
};

/** Async task handling the skin weight buffer generation */
class FSkinWeightProfileManagerAsyncTask
{
	FSkinWeightProfileManager* Owner;

public:
	FSkinWeightProfileManagerAsyncTask(FSkinWeightProfileManager* InOwner)
		: Owner(InOwner)
	{

	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSkinWeightProfileManagerAsyncTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
#if WITH_EDITOR
		// Force task to run on GT since it can cause a stall waiting for RT
		return ENamedThreads::GameThread;
#else

#if PLATFORM_ANDROID
		return IsRunningRHIInSeparateThread() ? ENamedThreads::RHIThread : ENamedThreads::ActualRenderingThread;
#else
		return ENamedThreads::AnyBackgroundHiPriTask;
#endif

#endif 		
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
};

USTRUCT()
struct FSkinWeightProfileManagerTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	//~ FTickFunction Interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed)  override;
	//~ FTickFunction Interface

	FSkinWeightProfileManager* Owner;
};

template<>
struct TStructOpsTypeTraits<FSkinWeightProfileManagerTickFunction> : public TStructOpsTypeTraitsBase2<FSkinWeightProfileManagerTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

class FSkinWeightProfileManager : public FTickableGameObject
{
protected: 
	friend FSkinWeightProfileManagerAsyncTask;

	static TMap<UWorld*, FSkinWeightProfileManager*> WorldManagers;
	static void OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS);
	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	static void OnPreWorldFinishDestroy(UWorld* World);
	static void OnWorldBeginTearDown(UWorld* World);

public: 
	static void OnStartup();
	static void OnShutdown();
	static FSkinWeightProfileManager* Get(UWorld* World);

	FSkinWeightProfileManager(UWorld* InWorld);
	virtual ~FSkinWeightProfileManager() {}

	void RequestSkinWeightProfile(FName InProfileName, USkinnedAsset* SkinnedAsset, UObject* Requester, FRequestFinished& Callback, int32 LODIndex = INDEX_NONE);
	void CancelSkinWeightProfileRequest(UObject* Requester);
	
	void DoTick(float DeltaTime, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
protected:	
	void CleanupRequest(const FSetProfileRequest& Request);
protected:
	TArray<FSetProfileRequest, TInlineAllocator<4>> CanceledRequest;
	TArray<FSetProfileRequest> PendingSetProfileRequests;
	TMap<TWeakObjectPtr<USkeletalMesh>, int32> PendingMeshes;
	FSkinWeightProfileManagerTickFunction TickFunction;
	int32 LastGamethreadProfileIndex;

	TWeakObjectPtr<UWorld> WeakWorld;

	FGraphEventRef AsyncTask;
public:
	virtual bool IsTickableWhenPaused() const override;
	virtual bool IsTickableInEditor() const override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

};
