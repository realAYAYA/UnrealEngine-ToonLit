// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshCardRepresentation.h
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/LockFreeList.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "Engine/EngineTypes.h"
#include "UObject/GCObject.h"
#include "AsyncCompilationHelpers.h"
#include "AssetCompilingManager.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "Templates/UniquePtr.h"
#include "DerivedMeshDataTaskUtils.h"
#include "Async/AsyncWork.h"

template <class T> class TLockFreePointerListLIFO;
class FSignedDistanceFieldBuildMaterialData;

namespace MeshCardRepresentation
{
	// Generation config
	extern ENGINE_API float GetMinDensity();
	extern ENGINE_API float GetNormalTreshold();

	// Debugging
	extern ENGINE_API bool IsDebugMode();
	extern ENGINE_API int32 GetDebugSurfelDirection();
};

class FLumenCardOBB
{
public:
	FVector3f Origin;
	FVector3f AxisX;
	FVector3f AxisY;
	FVector3f AxisZ;
	FVector3f Extent;

	void Reset()
	{
		Origin = FVector3f::ZeroVector;
		AxisX = FVector3f::ZeroVector;
		AxisY = FVector3f::ZeroVector;
		AxisZ = FVector3f::ZeroVector;
		Extent = FVector3f::ZeroVector;
	}

	FVector3f GetDirection() const
	{
		return AxisZ;
	}

	FMatrix44f GetCardToLocal() const
	{
		FMatrix44f CardToLocal;
		CardToLocal.SetIdentity();
		CardToLocal.SetAxes(&AxisX, &AxisY, &AxisZ, &Origin);
		return CardToLocal;
	}

	inline FVector3f RotateCardToLocal(FVector3f Vector3) const
	{
		return Vector3.X * AxisX + Vector3.Y * AxisY + Vector3.Z * AxisZ;
	}

	inline FVector3f RotateLocalToCard(FVector3f Vector3) const
	{
		return FVector3f(Vector3 | AxisX, Vector3 | AxisY, Vector3 | AxisZ);
	}

	inline FVector3f TransformLocalToCard(FVector3f LocalPosition) const
	{
		FVector3f Offset = LocalPosition - Origin;
		return FVector3f(Offset | AxisX, Offset | AxisY, Offset | AxisZ);
	}

	inline FVector3f TransformCardToLocal(FVector3f CardPosition) const
	{
		return Origin + CardPosition.X * AxisX + CardPosition.Y * AxisY + CardPosition.Z * AxisZ;
	}

	float ComputeSquaredDistanceToPoint(FVector3f WorldPosition) const
	{
		FVector3f CardPositon = TransformLocalToCard(WorldPosition);
		return ::ComputeSquaredDistanceFromBoxToPoint(-Extent, Extent, CardPositon);
	}

	FLumenCardOBB Transform(FMatrix44f LocalToWorld) const
	{
		FLumenCardOBB WorldOBB;
		WorldOBB.Origin = LocalToWorld.TransformPosition(Origin);

		const FVector3f ScaledXAxis = LocalToWorld.TransformVector(AxisX);
		const FVector3f ScaledYAxis = LocalToWorld.TransformVector(AxisY);
		const FVector3f ScaledZAxis = LocalToWorld.TransformVector(AxisZ);
		const float XAxisLength = ScaledXAxis.Size();
		const float YAxisLength = ScaledYAxis.Size();
		const float ZAxisLength = ScaledZAxis.Size();

		// #lumen_todo: fix axisX flip cascading into entire card code
		WorldOBB.AxisY = ScaledYAxis / FMath::Max(YAxisLength, UE_DELTA);
		WorldOBB.AxisZ = ScaledZAxis / FMath::Max(ZAxisLength, UE_DELTA);
		WorldOBB.AxisX = FVector3f::CrossProduct(WorldOBB.AxisZ, WorldOBB.AxisY);
		FVector3f::CreateOrthonormalBasis(WorldOBB.AxisX, WorldOBB.AxisY, WorldOBB.AxisZ);

		WorldOBB.Extent = Extent * FVector3f(XAxisLength, YAxisLength, ZAxisLength);
		WorldOBB.Extent.Z = FMath::Max(WorldOBB.Extent.Z, 1.0f);

		return WorldOBB;
	}

	FBox GetBox() const
	{
		FVector BoxMin(AxisX.GetAbs() * -Extent.X + AxisY.GetAbs() * -Extent.Y + AxisZ.GetAbs() * -Extent.Z + Origin);
		FVector BoxMax(AxisX.GetAbs() * +Extent.X + AxisY.GetAbs() * +Extent.Y + AxisZ.GetAbs() * +Extent.Z + Origin);
		return FBox(BoxMin, BoxMax);
	}

	friend FArchive& operator<<(FArchive& Ar, FLumenCardOBB& Data)
	{
		Ar << Data.AxisX;
		Ar << Data.AxisY;
		Ar << Data.AxisZ;
		Ar << Data.Origin;
		Ar << Data.Extent;
		return Ar;
	}
};

class FLumenCardBuildData
{
public:
	FLumenCardOBB OBB;
	uint8 AxisAlignedDirectionIndex;

	friend FArchive& operator<<(FArchive& Ar, FLumenCardBuildData& Data)
	{
		// Note: this is derived data, no need for versioning (bump the DDC guid)
		Ar << Data.OBB;
		Ar << Data.AxisAlignedDirectionIndex;
		return Ar;
	}
};

class FLumenCardBuildDebugData
{
public:
	enum class ESurfelType : uint8
	{
		Valid,
		Invalid,

		Idle,
		Cluster,
		Used
	};

	struct FRay
	{
		FVector3f RayStart;
		FVector3f RayEnd;
		bool bHit;
	};

	struct FSurfel
	{
		FVector3f Position;
		FVector3f Normal;
		float Coverage = 0.0f;
		float Visibility = 1.0f;
		int32 SourceSurfelIndex = -1;
		ESurfelType Type;
	};

	struct FSurfelCluster
	{
		TArray<FSurfel> Surfels;
		TArray<FRay> Rays;
	};

	TArray<FSurfel> Surfels;
	TArray<FRay> SurfelRays;
	TArray<FSurfelCluster> Clusters;
	int32 NumSurfels = 0;

	void Init()
	{
		Surfels.Reset();
		SurfelRays.Reset();
		Clusters.Reset();
		NumSurfels = 0;
	}
};

class FMeshCardsBuildData
{
public:
	FBox Bounds;
	TArray<FLumenCardBuildData> CardBuildData;

	// Temporary debug visualization data, don't serialize
	FLumenCardBuildDebugData DebugData;

	friend FArchive& operator<<(FArchive& Ar, FMeshCardsBuildData& Data)
	{
		// Note: this is derived data, no need for versioning (bump the DDC guid)
		Ar << Data.Bounds;
		Ar << Data.CardBuildData;
		return Ar;
	}
};

// Unique id per card representation data instance.
class FCardRepresentationDataId
{
public:
	uint32 Value = 0;

	bool IsValid() const
	{
		return Value != 0;
	}

	bool operator==(FCardRepresentationDataId B) const
	{
		return Value == B.Value;
	}

	friend uint32 GetTypeHash(FCardRepresentationDataId DataId)
	{
		return GetTypeHash(DataId.Value);
	}
};

/** Card representation payload and output of the mesh build process. */
class FCardRepresentationData : public FDeferredCleanupInterface
{
public:

	FMeshCardsBuildData MeshCardsBuildData;

	FCardRepresentationDataId CardRepresentationDataId;

	FCardRepresentationData() 
	{
		// 0 means invalid id.
		static uint32 NextCardRepresentationId = 0;
		++NextCardRepresentationId;
		CardRepresentationDataId.Value = NextCardRepresentationId;
	}

	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));
	}

	SIZE_T GetResourceSizeBytes() const
	{
		FResourceSizeEx ResSize;
		GetResourceSizeEx(ResSize);
		return ResSize.GetTotalMemoryBytes();
	}

#if WITH_EDITORONLY_DATA

	void CacheDerivedData(const FString& InDDCKey, const ITargetPlatform* TargetPlatform, UStaticMesh* Mesh, UStaticMesh* GenerateSource, int32 MaxLumenMeshCards, bool bGenerateDistanceFieldAsIfTwoSided, FSourceMeshDataForDerivedDataTask* OptionalSourceMeshData);

#endif

	friend FArchive& operator<<(FArchive& Ar,FCardRepresentationData& Data)
	{
		// Note: this is derived data, no need for versioning (bump the DDC guid)
		Ar << Data.MeshCardsBuildData;
		return Ar;
	}
};


class FAsyncCardRepresentationTask;
class FAsyncCardRepresentationTaskWorker : public FNonAbandonableTask
{
public:
	FAsyncCardRepresentationTaskWorker(FAsyncCardRepresentationTask& InTask)
		: Task(InTask)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCardRepresentationTaskWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();

private:
	FAsyncCardRepresentationTask& Task;
};

class FAsyncCardRepresentationTask
{
public:
	bool bSuccess = false;

#if WITH_EDITOR
	TArray<FSignedDistanceFieldBuildMaterialData> MaterialBlendModes;
#endif

	FSourceMeshDataForDerivedDataTask SourceMeshData;
	bool bGenerateDistanceFieldAsIfTwoSided = false;
	int32 MaxLumenMeshCards = 0;
	UStaticMesh* StaticMesh = nullptr;
	UStaticMesh* GenerateSource = nullptr;
	FString DDCKey;
	FCardRepresentationData* GeneratedCardRepresentation;
	TUniquePtr<FAsyncTask<FAsyncCardRepresentationTaskWorker>> AsyncTask = nullptr;
};

/** Class that manages asynchronous building of mesh distance fields. */
class FCardRepresentationAsyncQueue : IAssetCompilingManager
{
public:

	ENGINE_API FCardRepresentationAsyncQueue();

	ENGINE_API virtual ~FCardRepresentationAsyncQueue();

	/** Adds a new build task. */
	ENGINE_API void AddTask(FAsyncCardRepresentationTask* Task);

	/** Cancel the build on this specific static mesh or block until it is completed if already started. */
	ENGINE_API void CancelBuild(UStaticMesh* StaticMesh);

	/** Cancel the build on these meshes or block until they are completed if already started. */
	ENGINE_API void CancelBuilds(const TSet<UStaticMesh*>& InStaticMeshes);

	/** Blocks the main thread until the async build are either canceled or completed. */
	ENGINE_API void CancelAllOutstandingBuilds();

	/** Blocks the main thread until the async build of the specified mesh is complete. */
	ENGINE_API void BlockUntilBuildComplete(UStaticMesh* InStaticMesh, bool bWarnIfBlocked);

	/** Blocks the main thread until all async builds complete. */
	ENGINE_API void BlockUntilAllBuildsComplete();

	/** Called once per frame, fetches completed tasks and applies them to the scene. */
	ENGINE_API void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;

	/** Blocks until it is safe to shut down (worker threads are idle). */
	ENGINE_API void Shutdown() override;

	int32 GetNumOutstandingTasks() const
	{
		FScopeLock Lock(&CriticalSection);
		return ReferencedTasks.Num();
	}

	/** Get the name of the asset type this compiler handles */
	ENGINE_API static FName GetStaticAssetTypeName();

private:
	friend FAsyncCardRepresentationTaskWorker;

	ENGINE_API FName GetAssetTypeName() const override;
	ENGINE_API FTextFormat GetAssetNameFormat() const override;
	ENGINE_API TArrayView<FName> GetDependentTypeNames() const override;
	ENGINE_API int32 GetNumRemainingAssets() const override;
	ENGINE_API void FinishAllCompilation() override;
	
	void ProcessPendingTasks();

	TUniquePtr<FQueuedThreadPool> ThreadPool;

	/** Builds a single task with the given threadpool.  Called from the worker thread. */
	void Build(FAsyncCardRepresentationTask* Task, class FQueuedThreadPool& ThreadPool);

	/** Change the priority of the background task. */
	void RescheduleBackgroundTask(FAsyncCardRepresentationTask* InTask, EQueuedWorkPriority InPriority);

	/** Task will be sent to a background worker. */
	void StartBackgroundTask(FAsyncCardRepresentationTask* Task);

	/** Cancel or finish any work for any task matching the predicate. */
	void CancelAndDeleteTaskByPredicate(TFunctionRef<bool(FAsyncCardRepresentationTask*)> ShouldCancelPredicate);

	/** Cancel or finish any work for the given task. */
	void CancelAndDeleteTask(const TSet<FAsyncCardRepresentationTask*>& Tasks);

	/** Return whether the task has become invalid and should be canceled (i.e. reference unreachable objects) */
	bool IsTaskInvalid(FAsyncCardRepresentationTask* Task) const;

	/** Used to cancel tasks that are not needed anymore when garbage collection occurs */
	void OnPostReachabilityAnalysis();

	/** Game-thread managed list of tasks in the async system. */
	TSet<FAsyncCardRepresentationTask*> ReferencedTasks;

	/** Tasks that are waiting on static mesh compilation to proceed */
	TSet<FAsyncCardRepresentationTask*> PendingTasks;

	/** Tasks that have completed processing. */
	TSet<FAsyncCardRepresentationTask*> CompletedTasks;

	FDelegateHandle PostReachabilityAnalysisHandle;

	class IMeshUtilities* MeshUtilities;

	mutable FCriticalSection CriticalSection;
	
	FAsyncCompilationNotification Notification;
};

/** Global build queue. */
extern ENGINE_API FCardRepresentationAsyncQueue* GCardRepresentationAsyncQueue;

extern ENGINE_API FString BuildCardRepresentationDerivedDataKey(const FString& InMeshKey);

extern ENGINE_API void BeginCacheMeshCardRepresentation(const ITargetPlatform* TargetPlatform, UStaticMesh* StaticMeshAsset, class FStaticMeshRenderData& RenderData, const FString& DistanceFieldKey, FSourceMeshDataForDerivedDataTask* OptionalSourceMeshData);