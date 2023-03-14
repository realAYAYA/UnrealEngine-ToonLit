// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObject.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Ticker.h"

#include "MuR/System.h"
#include "MuR/Image.h"

// This define could come from MuR/System.h
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	#include "Tasks/Task.h"
#else
	#include "Async/TaskGraphInterfaces.h"
#endif


//! An operation to be performed by Mutable. This could be creating or updating an instance, releasing resources, changing an LOD, etc.
//! Operations may be done in several tasks in several thread or across frames.
class FMutableOperation 
{
	/** Instance parameters at the time of the operation request. */
	mu::ParametersPtr Parameters; 

	bool bBuildParameterDecorations = false;
	bool bMeshNeedsUpdate = false;

	/** Protected constructor. */
	FMutableOperation() {}

public:


	static FMutableOperation CreateInstanceUpdate(UCustomizableObjectInstance* COInstance, bool bInNeverStream, int32 MipsToSkip);
	static FMutableOperation CreateInstanceDiscard(UCustomizableObjectInstance* COInstance);


	enum class EOperationType
	{
		// Create mutable resources for a new instance or to update an exiting one.
		Update,

		// Discard the resources of an instance.
		Discard
	};

	// Type of the operation
	EOperationType Type;

	bool bStarted = false;

	bool bNeverStream = false;

	/** If this is non-zero, the instance images will be generated with a certain amount of mips starting from the smallest. Otherwise, the full images will be generated. 
	 * This is used for both Update and UpdateImage operations. 
	 */
	int32 MipsToSkip = 0;

	// Weak reference to the instance we are operating on.
	// It is weak because we don't want to lock it in case it becomes irrelevant in the game while operations are pending and it needs to be destroyed.
	TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	/** Hash of the UCustomizableObjectInstance::Descriptor (state of the instance parameters + state). */
	uint32 InstanceDescriptorHash;

	//! This is used to calculate stats.
	double StartUpdateTime = 0.0;

	//!
	bool IsBuildParameterDecorations() const
	{
		return bBuildParameterDecorations;
	}

	// \TODO: This should be intercepted earlier.
	// In case Mutable Compilation has been disabled, do all the corresponding steps to assign the reference mesh of the customizable object
	void MutableIsDisabledCase();

	/** Read-only access to the mutable instance parameters for this operation. */
	mu::ParametersPtrConst GetParameters() const
	{
		return Parameters;
	}
};



struct FMutableQueueElem
{
	// Priority for mutable update queue, Low is the normal distance-based priority, High is normally used for discards and Mid for LOD downgrades
	enum EQueuePriorityType { High, Med, Med_Low, Low };

	EQueuePriorityType PriorityType;
	float Priority;
	TSharedPtr<FMutableOperation> Operation;
	bool bIsDiscardResources;

	static FMutableQueueElem Create(TSharedPtr<FMutableOperation> InOperation, EQueuePriorityType NewPriorityType, double NewPriority)
	{
		FMutableQueueElem MutableQueueElem;

		MutableQueueElem.PriorityType = NewPriorityType;
		MutableQueueElem.Priority = NewPriority;
		MutableQueueElem.Operation = InOperation;
		MutableQueueElem.bIsDiscardResources = (InOperation->Type==FMutableOperation::EOperationType::Discard);

		return MutableQueueElem;
	}

	friend bool operator <(const FMutableQueueElem& A, const FMutableQueueElem& B)
	{
		if (A.PriorityType < B.PriorityType)
		{
			return true;
		}
		else if (A.PriorityType > B.PriorityType)
		{
			return false;
		}
		else
		{
			if (A.bIsDiscardResources && !B.bIsDiscardResources)
			{
				return true;
			}
			else if (!A.bIsDiscardResources && B.bIsDiscardResources)
			{
				return false;
			}

			return A.Priority < B.Priority;
		}
	}
};


/** Instances operations queue.
 *
 * The queue will only contain a single operation per UCustomizableObjectInstance.
 * If there is already an operation it will be replaced. */
class FMutableQueue
{
	TArray<FMutableQueueElem> Array;

public:
	bool IsEmpty() const;

	int Num() const;

	void Enqueue(const FMutableQueueElem& TaskToEnqueue);

	void Dequeue(FMutableQueueElem* DequeuedTask);

	void ChangePriorities();

	void UpdatePriority(const UCustomizableObjectInstance* Instance);

	const FMutableQueueElem* Get(const UCustomizableObjectInstance* Instance) const;
	
	void Sort();
};


struct FMutableImageCacheKey
{
	mu::RESOURCE_ID Resource = 0;
	int32 SkippedMips = 0;

	FMutableImageCacheKey(mu::RESOURCE_ID InResource, int32 InSkippedMips)
		: Resource(InResource), SkippedMips(InSkippedMips) {}

	inline bool operator==(const FMutableImageCacheKey& Other) const
	{
		return Resource == Other.Resource && SkippedMips == Other.SkippedMips;
	}
};


inline uint32 GetTypeHash(const FMutableImageCacheKey& Key)
{
	return HashCombine(GetTypeHash(Key.Resource), GetTypeHash(Key.SkippedMips));
}


// Cache of weak references to generated resources for one single model
struct FMutableResourceCache
{
	TWeakObjectPtr<const UCustomizableObject> Object;
	TMap<mu::RESOURCE_ID, TWeakObjectPtr<USkeletalMesh> > Meshes;
	TMap<FMutableImageCacheKey, TWeakObjectPtr<UTexture2D> > Images;

	void Clear()
	{
		Meshes.Reset();
		Images.Reset();
	}
};


DECLARE_DELEGATE(FMutableTaskDelegate);
struct FMutableTask
{
	/** Actual function to perform in this task. */
	FMutableTaskDelegate Function;

	/** We can have 2 types of depencies:
		From the new task system: */
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	UE::Tasks::FTask Dependency;
#endif

	/** From the traditional event graph system, still used to wait for async loads. */
	FGraphEventRef GraphDependency0;
	FGraphEventRef GraphDependency1;

	/** Check if all the dependencies of this task have been completed. */
	inline bool AreDependenciesComplete() const
	{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		return (!Dependency.IsValid() || Dependency.IsCompleted())
			&&
			(!GraphDependency0 || GraphDependency0->IsComplete())
			&&
			(!GraphDependency1 || GraphDependency1->IsComplete());
#else
		return 
			(!GraphDependency0 || GraphDependency0->IsComplete())
			&&
			(!GraphDependency1 || GraphDependency1->IsComplete());
#endif
	}

	/** Free the handles for any dependency of this task. */
	inline void ClearDependencies()
	{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		Dependency = {};
#endif
		GraphDependency0 = nullptr;
		GraphDependency1 = nullptr;
	}
};


// Mutable data generated during the update steps.
// We keep it from begin to end update, and it is used in several steps.
// TODO: Flatten this structure into 4 arrays and use indices in fixed-size structs instead of subarrays
struct FInstanceUpdateData
{
	struct FImage
	{
		FString Name;
		mu::RESOURCE_ID ImageID;
		uint16 FullImageSizeX, FullImageSizeY;
		mu::ImagePtrConst Image;
		TWeakObjectPtr<UTexture2D> Cached;
	};

	struct FVector
	{
		FString Name;
		FLinearColor Vector;
	};

	struct FScalar
	{
		FString Name;
		float Scalar;
	};

	struct FSurface
	{
		/** Range in the Images array */
		uint16 FirstImage = 0;
		uint16 ImageCount = 0;

		/** Range in the Vectors array */
		uint16 FirstVector = 0;
		uint16 VectorCount = 0;

		/** Range in the Scalar array */
		uint16 FirstScalar = 0;
		uint16 ScalarCount = 0;

		uint32 MaterialIndex = 0;

		/** Id of the surface in the mutable core instance. */
		uint32 SurfaceId = 0;
	};

	struct FComponent
	{
		uint16 Id = 0;
		
		mu::MeshPtrConst Mesh;

		/** Range in the Surfaces array */
		uint16 FirstSurface = 0;
		uint16 SurfaceCount = 0;

		// \TODO: Flatten
		TArray<uint16> ActiveBones;
		/** Range in the external Bones array */
		//uint32 FirstActiveBone;
		//uint32 ActiveBoneCount;

		// \TODO: Flatten
		TArray<uint16> BoneMap;
		/** Range in the external Bones array */
		//uint32 FirstBoneMap;
		//uint32 BoneMapCount;
	};

	struct FLOD
	{
		/** Range in the Components array */
		uint16 FirstComponent = 0;
		uint16 ComponentCount = 0;
	};

	TArray<FLOD> LODs;
	TArray<FComponent> Components;
	TArray<FSurface> Surfaces;
	TArray<FImage> Images;
	TArray<FVector> Vectors;
	TArray<FScalar> Scalars;

	struct FSkeletonData
	{
		int16 ComponentIndex = INDEX_NONE;

		TArray<uint32> SkeletonIds;

		TArray<FName> BoneNames;
		TMap<FName, FMatrix44f> BoneMatricesWithScale;
	};

	TArray<FSkeletonData> Skeletons;

	/** */
	void Clear()
	{
		LODs.Empty();
		Components.Empty();
		Surfaces.Empty();
		Images.Empty();
		Scalars.Empty();
		Vectors.Empty();
		Skeletons.Empty();
	}
};


// Mutable data generated during the update steps.
struct FParameterDecorationsUpdateData
{
	struct FParameterDesc
	{
		TArray<mu::ImagePtrConst> Images;
	};

	//! Information about additional images used for parameter UI decoration
	TArray<FParameterDesc> Parameters;

	void Clear()
	{
		Parameters.Empty();
	}
};


struct FTextureCoverageQueryData
{
	uint32 CoveredTexels;
	uint32 MaskedOutCoveredTexels;
	uint32 TotalTexels;
	FString MaskOutChannelName;

	FTextureCoverageQueryData() : CoveredTexels(0), MaskedOutCoveredTexels(0), TotalTexels(0) { }

	float GetCoverage() const { return TotalTexels > 0 ? (float(CoveredTexels) / TotalTexels) : 0.f; }
	float GetMaskedOutCoverage() const { return CoveredTexels > 0 ? (float(MaskedOutCoveredTexels) / CoveredTexels) : 0.f; }
};


struct FPendingTextureCoverageQuery
{
	FString KeyName;
	uint32 MaterialIndex = 0;
	FTexturePlatformData* PlatformData = nullptr;
};


/** Runtime data used during a mutable instance update */
struct FMutableOperationData
{
	FInstanceUpdateData InstanceUpdateData;
	FParameterDecorationsUpdateData ParametersUpdateData;
	TArray<int> RelevantParametersInProgress;

	/** This option comes from the operation request */
	bool bNeverStream = false;
	/** This option comes from the operation request. It is used to reduce the number of mipmaps that mutable must generate for images.  */
	int32 MipsToSkip = 0;

	mu::Instance::ID InstanceID = 0;
	int32 CurrentMinLOD = 0;
	int32 CurrentMaxLOD = 0;
	int32 NumLODsAvailable = 0;

	TMap<FString, FTextureCoverageQueryData> TextureCoverageQueries_MutableThreadParams;
	TMap<FString, FTextureCoverageQueryData> TextureCoverageQueries_MutableThreadResults;

	TMap<uint32, FTexturePlatformData*> ImageToPlatformDataMap;

	/** This list of queries is generated in the update mutable task, and consumed later in the game thread. */
	TArray<FPendingTextureCoverageQuery> PendingTextureCoverageQueries;

	/** */
	mu::Ptr<const mu::Parameters> MutableParameters;
	int32 State = 0;

#if WITH_EDITOR
	/** Used for profiling in the editor. */
	uint32 MutableRuntimeCycles = 0;
#endif 
};


/** Runtime data used during a mutable instance update */
struct FMutableReleasePlatformOperationData
{
	TMap<uint32, FTexturePlatformData*> ImageToPlatformDataMap;
};


class FCustomizableObjectSystemPrivate : public FGCObject
{
public:

	// Singleton for the unreal mutable system.
	static UCustomizableObjectSystem* SSystem;

	// Pointer to the lower level mutable system that actually does the work.
	mu::SystemPtr MutableSystem;

	// Store the last streaming memory size in bytes, to change it when it is safe.
	uint64_t LastStreamingMemorySize = 0;

	// This object is responsible for streaming data to the MutableSystem.
	// Non-owned reference
	class FUnrealMutableModelBulkStreamer* Streamer = nullptr;

	// This object is responsible for providing custom images to mutable models (for image parameters)
	// This object is called from the mutable thread, and it should only access data already safely submitted from
	// the game thread and stored in FUnrealMutableImageProvider::GlobalExternalImages.
	// Non-owned reference
	class FUnrealMutableImageProvider* ImageProvider = nullptr;

	// Cache of weak references to generated resources to see if they can be reused.
	TArray<FMutableResourceCache> ModelResourcesCache;

	// List of textures currently cached and valid for the current object that we are operating on.
	// This array gets generated when the object cached resources are protected in SetResourceCacheProtected
	// from the game thread, and it is read from the Mutable thread only while updating the instance.
	TArray<mu::RESOURCE_ID> ProtectedObjectCachedImages;

	FMutableQueue MutableOperationQueue;

	// Queue of game-thread tasks that need to be executed for the current operation
	TQueue<FMutableTask> PendingTasks;

	static int32 EnableMutableProgressiveMipStreaming;

	/** */
	inline void AddGameThreadTask(const FMutableTask& Task)
	{
		PendingTasks.Enqueue(Task);
	}

	/** */
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	template<typename TaskBodyType>
	inline UE::Tasks::FTask AddMutableThreadTask(const TCHAR* DebugName, TaskBodyType&& TaskBody, UE::Tasks::ETaskPriority Priority)
	{
		// This could be called from the game thread or from a worker thread for the texture streaming system
		//check(IsInGameThread());
		FScopeLock Lock(&MutableTaskLock);

		if (LastMutableTask.IsValid())
		{
			LastMutableTask = UE::Tasks::Launch(DebugName, MoveTemp(TaskBody), LastMutableTask, Priority);
		}
		else
		{
			LastMutableTask = UE::Tasks::Launch(DebugName, MoveTemp(TaskBody), Priority);
		}
		return LastMutableTask;
	}
#else
	template<typename TaskBodyType>
	inline FGraphEventRef AddMutableThreadTask(const TCHAR* DebugName, TaskBodyType&& TaskBody, UE::Tasks::ETaskPriority Priority)
	{
		// This could be called from the game thread or from a worker thread for the texture streaming system
		//check(IsInGameThread());
		FScopeLock Lock(&MutableTaskLock);

		// Chain to pending tasks of the mutable thread if any. This ensures exclusive access to mutable.
		FGraphEventArray Prerequisites;
		if (LastMutableTask)
		{
			Prerequisites.Add(LastMutableTask);
		}

		LastMutableTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
			MoveTemp(TaskBody),
			TStatId{},
			&Prerequisites, 
			ENamedThreads::AnyThread);
		LastMutableTask->SetDebugName(DebugName);

		return LastMutableTask;
	}

	/** This should only be used when shutting down. */
	inline void WaitForMutableTasks()
	{
		FScopeLock Lock(&MutableTaskLock);

		if (LastMutableTask.IsValid())
		{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
			LastMutableTask.Wait();
#else
			LastMutableTask->Wait();
#endif
			LastMutableTask = {};
		}
	}

	inline void ClearMutableTaskIfDone()
	{
		FScopeLock Lock(&MutableTaskLock);

#ifdef MUTABLE_USE_NEW_TASKGRAPH
		if (LastMutableTask.IsValid() && LastMutableTask.IsCompleted())
		{
			LastMutableTask = {};
		}
#else
		if (LastMutableTask.IsValid() && LastMutableTask->IsComplete())
		{
			LastMutableTask = nullptr;
		}
#endif
	}


	template<typename TaskBodyType>
	inline void AddAnyThreadTask(const TCHAR* DebugName, TaskBodyType&& TaskBody, UE::Tasks::ETaskPriority Priority)
	{
		check(IsInGameThread());

		FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
			MoveTemp(TaskBody),
			TStatId{},
			nullptr,
			ENamedThreads::AnyThread);
		Task->SetDebugName(TEXT("Mutable_Anythread"));
	}
#endif

	//
	//TSharedPtr<FMutableOperationData> CurrentOperationData;


	/** FSerializableObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectSystemPrivate");
	}


	// Remove references to cached objects that have been deleted in the unreal
	// side, and cannot be cached anyway.
	// This should only happen in the mutable thread
	void CleanupCache()
	{
		for (int ModelIndex=0; ModelIndex<ModelResourcesCache.Num();)
		{
			if (!ModelResourcesCache[ModelIndex].Object.IsValid(false,true))
			{
				// The whole object has been destroyed. Remove everything.
				ModelResourcesCache.RemoveAtSwap(ModelIndex);
			}
			else
			{
				// \todo: Free invalid references to specific resources
				++ModelIndex;
			}
		}
	}


	// This should only happen in the game thread
	FMutableResourceCache& GetObjectCache(const UCustomizableObject* Object)
	{
		check(IsInGameThread());

		// Not mandatory, but a good place for a cleanup
		CleanupCache();

		for (int ModelIndex = 0; ModelIndex < ModelResourcesCache.Num(); ++ModelIndex)
		{
			if (ModelResourcesCache[ModelIndex].Object==Object)
			{
				return ModelResourcesCache[ModelIndex];
			}
		}
		
		// Not found, create and add it.
		ModelResourcesCache.Push(FMutableResourceCache());
		ModelResourcesCache.Last().Object = Object;
		return ModelResourcesCache.Last();
	}


	void AddTextureReference(uint32 TextureId)
	{
		uint32& CountRef = TextureReferenceCount.FindOrAdd(TextureId);

		CountRef++;
	}

	
	// Returns true if the texture's references become zero
	bool RemoveTextureReference(uint32 TextureId)
	{
		uint32* CountPtr = TextureReferenceCount.Find(TextureId);

		if (CountPtr && *CountPtr > 0)
		{
			(*CountPtr)--;

			if (*CountPtr == 0)
			{
				TextureReferenceCount.Remove(TextureId);

				return true;
			}
		}
		else
		{
			ensure(false); // Mutable texture reference count is incorrect
			TextureReferenceCount.Remove(TextureId);
		}

		return false;
	}


	bool TextureHasReferences(uint32 TextureId) const
	{
		const uint32* CountPtr = TextureReferenceCount.Find(TextureId);

		if (CountPtr && *CountPtr > 0)
		{
			return true;
		}

		return false;
	}


	// Init the async Skeletal Mesh creation/update
	void InitUpdateSkeletalMesh(UCustomizableObjectInstance* Public, FMutableQueueElem::EQueuePriorityType Priority);
		
	// Init an async and safe release of the UE4 and Mutable resources used by the instance without actually destroying the instance, for example if it's very far away
	void InitDiscardResourcesSkeletalMesh(UCustomizableObjectInstance* InCustomizableObjectInstance);
	
	bool IsReplaceDiscardedWithReferenceMeshEnabled() const { return bReplaceDiscardedWithReferenceMesh; }
	void SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled) { bReplaceDiscardedWithReferenceMesh = bIsEnabled; }

	int32 GetCountAllocatedSkeletalMesh() { return CountAllocatedSkeletalMesh; }

	bool bCompactSerialization = true;

	bool bReplaceDiscardedWithReferenceMesh = false;
	bool bReleaseTexturesImmediately = false;

	bool bSupport16BitBoneIndex = false;

	// Statistics: Total time spent building instances
	int32 TotalBuildMs = 0;

	// Statistics: Total number of instances built or updated.
	int32 TotalBuiltInstances = 0;

	// Statistics: Number of instances alive (built)
	int32 NumInstances = 0;

	// Statistics: number of CustomizableObjectInstances waiting to be updated.
	int32 NumPendingInstances = 0;

	// Statistics: Total number of CustomizableObjectInstances, including not built.
	int32 TotalInstances = 0;

	// Statistics: total memory in bytes used for generated textures
	int64_t TextureMemoryUsed = 0;

	mutable uint32 CountAllocatedSkeletalMesh = 0;

	static FCustomizableObjectCompilerBase* (*NewCompilerFunc)();

	void CreatedTexture(UTexture2D* Texture);

	// \TODO: Remove this array if we are not gathering stats!
	TArray<TWeakObjectPtr<class UTexture2D>> TextureTrackerArray;
	TMap<uint32, uint32> TextureReferenceCount; // Keeps a count of texture usage to decide if they have to be blocked from GC during an update

	// This is protected from GC by AddReferencedObjects
	UCustomizableObjectInstance* CurrentInstanceBeingUpdated = nullptr;

	TSharedPtr<FMutableOperation> CurrentMutableOperation = nullptr;

	// Handle to the registered TickDelegate.
	FTSTicker::FDelegateHandle TickDelegateHandle;
	FTickerDelegate TickDelegate;

	//! Update the stats logged in unreal's stats system. 
	void UpdateStats();

	// Important!!! Never call when there's a Begin Update thread running!
	void ReleasePendingMutableInstances();

	// Check and update the streaming memory limit. Only safe from game thread and when the mutable thread is idle.
	void UpdateStreamingLimit();

private:

	/** Access to this must be protected with this. */
	mutable FCriticalSection MutableTaskLock;

#ifdef MUTABLE_USE_NEW_TASKGRAPH
	UE::Tasks::FTask LastMutableTask;
#else
	FGraphEventRef LastMutableTask = nullptr;
#endif


};


void ConvertImage(UTexture2D* Texture, mu::ImagePtrConst MutableImage, const struct FMutableModelImageProperties& Props);



