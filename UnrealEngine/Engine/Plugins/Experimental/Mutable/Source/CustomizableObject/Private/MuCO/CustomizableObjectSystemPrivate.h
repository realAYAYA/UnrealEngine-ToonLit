// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Queue.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "Containers/Ticker.h"

#include "MuCO/CustomizableObjectInstanceDescriptor.h"
#include "MuR/Mesh.h"
#include "MuR/Parameters.h"
#include "MuR/System.h"
#include "MuR/Image.h"
#include "UObject/GCObject.h"
#include "WorldCollision.h"
#include "MuCO/FMutableTaskGraph.h"

// This define could come from MuR/System.h
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	#include "Tasks/Task.h"
#else
	#include "Async/TaskGraphInterfaces.h"
#endif

class UCustomizableObjectSystem;
namespace LowLevelTasks { enum class ETaskPriority : int8; }
struct FTexturePlatformData;

//! An operation to be performed by Mutable. This could be creating or updating an instance, releasing resources, changing an LOD, etc.
//! Operations may be done in several tasks in several thread or across frames.
class FMutableOperation 
{
	/** Instance parameters at the time of the operation request. */
	mu::ParametersPtr Parameters; 
	
	TArray<FName> TextureParameters;

	bool bBuildParameterRelevancy = false;
	bool bMeshNeedsUpdate = false;

	/** Protected constructor. */
	FMutableOperation() {}

public:

	FMutableOperation(const FMutableOperation&);
	FMutableOperation(FMutableOperation&&) = default;
	FMutableOperation& operator=(const FMutableOperation&);
	FMutableOperation& operator=(FMutableOperation&&) = default;

	~FMutableOperation();

	static FMutableOperation CreateInstanceUpdate(UCustomizableObjectInstance* COInstance, bool bInNeverStream, int32 MipsToSkip, const FInstanceUpdateDelegate* UpdateCallback);

	bool bStarted = false;

	bool bForceGenerateAllLODs = false;

	bool bNeverStream = false;

	/** If this is non-zero, the instance images will be generated with a certain amount of mips starting from the smallest. Otherwise, the full images will be generated. 
	 * This is used for both Update and UpdateImage operations. 
	 */
	int32 MipsToSkip = 0;

	// Weak reference to the instance we are operating on.
	// It is weak because we don't want to lock it in case it becomes irrelevant in the game while operations are pending and it needs to be destroyed.
	TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	/** Hash of the UCustomizableObjectInstance::Descriptor at the time of the update request. */
	FDescriptorRuntimeHash InstanceDescriptorRuntimeHash;

	//! This is used to calculate stats.
	double StartUpdateTime = 0.0;

	/** Instance optimization state. */
	int32 State;

	/** Only used in the IDRelease operation type */
	mu::Instance::ID IDToRelease;

	FInstanceUpdateDelegate UpdateCallback;
	
	bool IsBuildParameterRelevancy() const
	{
		return bBuildParameterRelevancy;
	}

	/** Read-only access to the mutable instance parameters for this operation. */
	mu::ParametersPtrConst GetParameters() const
	{
		return Parameters;
	}
};


struct FMutablePendingInstanceUpdate
{
	EQueuePriorityType PriorityType = EQueuePriorityType::Low;

	TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;
	FCustomizableObjectInstanceDescriptor InstanceDescriptor;

	double SecondsAtUpdate = 0;
	FInstanceUpdateDelegate* Callback = nullptr;
	bool bNeverStream = false;

	/** If this is non-zero, the instance images will be generated with a certain amount of mips starting from the smallest. Otherwise, the full images will be generated.
	 * This is used for both Update and UpdateImage operations.
	 */
	int32 MipsToSkip = 0;

	FMutablePendingInstanceUpdate(UCustomizableObjectInstance* InCustomizableObjectInstance)
	{
		check(InCustomizableObjectInstance);
		CustomizableObjectInstance = InCustomizableObjectInstance;
	}

	FMutablePendingInstanceUpdate(UCustomizableObjectInstance* InCustomizableObjectInstance, EQueuePriorityType NewPriorityType, 
		                          FInstanceUpdateDelegate* InCallback, bool bInNeverStream, int32 InMipsToSkip)
	{
		PriorityType = NewPriorityType;

		check(InCustomizableObjectInstance);
		CustomizableObjectInstance = InCustomizableObjectInstance;
		InstanceDescriptor = CustomizableObjectInstance->GetDescriptor();

		SecondsAtUpdate = FPlatformTime::Seconds();
		Callback = InCallback;
		bNeverStream = bInNeverStream;
		MipsToSkip = InMipsToSkip;
	}

	friend bool operator ==(const FMutablePendingInstanceUpdate& A, const FMutablePendingInstanceUpdate& B)
	{
		return A.CustomizableObjectInstance.HasSameIndexAndSerialNumber(B.CustomizableObjectInstance);
	}

	friend bool operator <(const FMutablePendingInstanceUpdate& A, const FMutablePendingInstanceUpdate& B)
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
			return A.SecondsAtUpdate < B.SecondsAtUpdate;
		}
	}
};


inline uint32 GetTypeHash(const FMutablePendingInstanceUpdate& Update)
{
	return GetTypeHash(Update.CustomizableObjectInstance.GetWeakPtrTypeHash());
}


struct FPendingInstanceUpdateKeyFuncs : BaseKeyFuncs<FMutablePendingInstanceUpdate, TWeakObjectPtr<UCustomizableObjectInstance>>
{
	FORCEINLINE static const TWeakObjectPtr<UCustomizableObjectInstance>& GetSetKey(const FMutablePendingInstanceUpdate& PendingUpdate)
	{
		return PendingUpdate.CustomizableObjectInstance;
	}

	FORCEINLINE static bool Matches(const TWeakObjectPtr<UCustomizableObjectInstance>& A, const TWeakObjectPtr<UCustomizableObjectInstance>& B)
	{
		return A.HasSameIndexAndSerialNumber(B);
	}

	FORCEINLINE static uint32 GetKeyHash(const TWeakObjectPtr<UCustomizableObjectInstance>& Identifier)
	{
		return GetTypeHash(Identifier.GetWeakPtrTypeHash());
	}
};


struct FMutablePendingInstanceDiscard
{
	TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	FMutablePendingInstanceDiscard(UCustomizableObjectInstance* InCustomizableObjectInstance)
	{
		CustomizableObjectInstance = InCustomizableObjectInstance;
	}

	friend bool operator ==(const FMutablePendingInstanceDiscard& A, const FMutablePendingInstanceDiscard& B)
	{
		return A.CustomizableObjectInstance.HasSameIndexAndSerialNumber(B.CustomizableObjectInstance);
	}
};


inline uint32 GetTypeHash(const FMutablePendingInstanceDiscard& Discard)
{
	return GetTypeHash(Discard.CustomizableObjectInstance.GetWeakPtrTypeHash());
}


struct FPendingInstanceDiscardKeyFuncs : BaseKeyFuncs<FMutablePendingInstanceUpdate, TWeakObjectPtr<UCustomizableObjectInstance>>
{
	FORCEINLINE static const TWeakObjectPtr<UCustomizableObjectInstance>& GetSetKey(const FMutablePendingInstanceDiscard& PendingDiscard)
	{
		return PendingDiscard.CustomizableObjectInstance;
	}

	FORCEINLINE static bool Matches(const TWeakObjectPtr<UCustomizableObjectInstance>& A, const TWeakObjectPtr<UCustomizableObjectInstance>& B)
	{
		return A.HasSameIndexAndSerialNumber(B);
	}

	FORCEINLINE static uint32 GetKeyHash(const TWeakObjectPtr<UCustomizableObjectInstance>& Identifier)
	{
		return GetTypeHash(Identifier.GetWeakPtrTypeHash());
	}
};


/** Instance updates queue.
 *
 * The queues will only contain a single operation per UCustomizableObjectInstance.
 * If there is already an operation it will be replaced. */
class FMutablePendingInstanceWork
{
	TSet<FMutablePendingInstanceUpdate, FPendingInstanceUpdateKeyFuncs> PendingInstanceUpdates;

	TSet<FMutablePendingInstanceDiscard, FPendingInstanceDiscardKeyFuncs> PendingInstanceDiscards;

	TSet<mu::Instance::ID> PendingIDsToRelease;

	int32 NumLODUpdatesLastTick = 0;

public:
	// Returns true if there are no pending instance updates. Doesn't take into account discards.
	bool ArePendingUpdatesEmpty() const;

	// Returns the number of pending instance updates, LOD Updates, discards and releases last tick.
	int32 Num() const;

	void SetLODUpdatesLastTick(int32 NumLODUpdates);

	// Adds a new instance update
	void AddUpdate(const FMutablePendingInstanceUpdate& UpdateToAdd);

	// Removes an instance update
	void RemoveUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance);

	const FMutablePendingInstanceUpdate* GetUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance) const;

	TSet<FMutablePendingInstanceUpdate, FPendingInstanceUpdateKeyFuncs>::TIterator GetUpdateIterator()
	{
		return PendingInstanceUpdates.CreateIterator();
	}

	TSet<FMutablePendingInstanceDiscard, FPendingInstanceDiscardKeyFuncs>::TIterator GetDiscardIterator()
	{
		return PendingInstanceDiscards.CreateIterator();
	}

	TSet<mu::Instance::ID>::TIterator GetIDsToReleaseIterator()
	{
		return PendingIDsToRelease.CreateIterator();
	}

	void AddDiscard(const FMutablePendingInstanceDiscard& TaskToEnqueue);
	void AddIDRelease(mu::Instance::ID IDToRelease);

	void RemoveAllUpdatesAndDiscardsAndReleases();
};


struct FMutableImageCacheKey
{
	mu::FResourceID Resource = 0;
	int32 SkippedMips = 0;

	FMutableImageCacheKey() {};

	FMutableImageCacheKey(mu::FResourceID InResource, int32 InSkippedMips)
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
	TMap<mu::FResourceID, TWeakObjectPtr<USkeletalMesh> > Meshes;
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
	
#ifdef MUTABLE_USE_NEW_TASKGRAPH
	UE::Tasks::FTask GraphDependency1;
#else
	FGraphEventRef GraphDependency1;
#endif

	/** Check if all the dependencies of this task have been completed. */
	inline bool AreDependenciesComplete() const
	{
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		return (!Dependency.IsValid() || Dependency.IsCompleted())
			&&
			(!GraphDependency0 || GraphDependency0->IsComplete())
			&&
			(GraphDependency1.IsCompleted());
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

#ifdef MUTABLE_USE_NEW_TASKGRAPH
		GraphDependency1 = {};
#else
		GraphDependency1 = nullptr;
#endif
	}
};


// Mutable data generated during the last update of an instance.
struct FInstanceGeneratedData
{
	struct FComponent
	{
		uint16 ComponentId = 0;

		/** True if it can be reused */
		bool bGenerated = false;

		mu::FResourceID MeshID;

		/** Range in the Surfaces array */
		uint16 FirstSurface = 0;
		uint16 SurfaceCount = 0;
	};

	struct FLOD
	{
		/** Range in the Components array */
		uint16 FirstComponent = 0;
		uint16 ComponentCount = 0;
	};

	TArray<FLOD> LODs;
	TArray<FComponent> Components;
	TArray<uint32> SurfaceIds;

	/** Clear data, called upon failing to generate a mesh and after recompiling the CO */
	void Clear()
	{
		LODs.Empty();
		Components.Empty();
		SurfaceIds.Empty();
	}
};


// Mutable data generated during the update steps.
// We keep it from begin to end update, and it is used in several steps.
struct FInstanceUpdateData
{
	struct FImage
	{
		FString Name;
		mu::FResourceID ImageID;
		
		// LOD of the ImageId. If the texture is shared between LOD, first LOD where this image can be found. 
		int32 BaseLOD;
		
		uint16 FullImageSizeX, FullImageSizeY;
		mu::Ptr<const mu::Image> Image;
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
		
		// True if the Mesh is valid or if we're reusing a component
		bool bGenerated = false;

		// Reuse component from a previously generated SkeletalMesh
		bool bReuseMesh = false;

		mu::FResourceID MeshID;
		mu::MeshPtrConst Mesh;

		/** Range in the Surfaces array */
		uint16 FirstSurface = 0;
		uint16 SurfaceCount = 0;

		// \TODO: Flatten
		TArray<uint16> ActiveBones;
		/** Range in the external Bones array */
		//uint32 FirstActiveBone;
		//uint32 ActiveBoneCount;

		/** Range in the external Bones array */
		uint32 FirstBoneMap = 0;
		uint32 BoneMapCount = 0;
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

	TArray<uint16> BoneMaps;

	struct FSkeletonData
	{
		int16 ComponentIndex = INDEX_NONE;

		TArray<uint16> SkeletonIds;

		TArray<uint16> BoneIds;
		TArray<FMatrix44f> BoneMatricesWithScale;
	};

	TArray<FSkeletonData> Skeletons;

	struct FNamedExtensionData
	{
		mu::ExtensionDataPtrConst Data;
		FName Name;
	};
	TArray<FNamedExtensionData> ExtendedInputPins;

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
		ExtendedInputPins.Empty();
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
	bool bCanReuseGeneratedData = false;
	FInstanceGeneratedData LastUpdateData;

	FInstanceUpdateData InstanceUpdateData;
	TArray<int> RelevantParametersInProgress;

	TArray<FString> LowPriorityTextures;

	/** This option comes from the operation request */
	bool bNeverStream = false;
	/** When this option is enabled it will reuse the Mutable core instance and its temp data between updates.  */
	bool bLiveUpdateMode = false;
	bool bReuseInstanceTextures = false;
	/** This option comes from the operation request. It is used to reduce the number of mipmaps that mutable must generate for images.  */
	int32 MipsToSkip = 0;

	mu::Instance::ID InstanceID = 0;
	int32 CurrentMinLOD = 0;
	int32 CurrentMaxLOD = 0;
	int32 NumLODsAvailable = 0;

	TArray<uint16> RequestedLODs;

	TMap<FString, FTextureCoverageQueryData> TextureCoverageQueries_MutableThreadParams;
	TMap<FString, FTextureCoverageQueryData> TextureCoverageQueries_MutableThreadResults;

	TMap<uint32, FTexturePlatformData*> ImageToPlatformDataMap;

	/** This list of queries is generated in the update mutable task, and consumed later in the game thread. */
	TArray<FPendingTextureCoverageQuery> PendingTextureCoverageQueries;

	/** */
	mu::Ptr<const mu::Parameters> MutableParameters;
	int32 State = 0;

	EUpdateResult UpdateResult;
	FInstanceUpdateDelegate UpdateCallback;

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
	mu::Ptr<mu::System> MutableSystem;

	/** Store the last streaming memory size in bytes, to change it when it is safe. */
	uint64 LastWorkingMemoryBytes = 0;
	uint32 LastGeneratedResourceCacheSize = 0;

	// This object is responsible for streaming data to the MutableSystem.
	TSharedPtr<class FUnrealMutableModelBulkReader> Streamer;

	// 
	TSharedPtr<class FUnrealExtensionDataStreamer> ExtensionDataStreamer;

	// This object is responsible for providing custom images to mutable models (for image parameters)
	// This object is called from the mutable thread, and it should only access data already safely submitted from
	// the game thread and stored in FUnrealMutableImageProvider::GlobalExternalImages.
	TSharedPtr<class FUnrealMutableImageProvider> ImageProvider;

	// Cache of weak references to generated resources to see if they can be reused.
	TArray<FMutableResourceCache> ModelResourcesCache;

	// List of textures currently cached and valid for the current object that we are operating on.
	// This array gets generated when the object cached resources are protected in SetResourceCacheProtected
	// from the game thread, and it is read from the Mutable thread only while updating the instance.
	TArray<mu::FResourceID> ProtectedObjectCachedImages;

	// The pending instance updates, discards or releases
	FMutablePendingInstanceWork MutablePendingInstanceWork;

	// Queue of game-thread tasks that need to be executed for the current operation
	TQueue<FMutableTask> PendingTasks;

	static int32 EnableMutableProgressiveMipStreaming;
	static int32 EnableMutableLiveUpdate;
	static int32 EnableReuseInstanceTextures;
	static int32 EnableMutableAnimInfoDebugging;
	static int32 EnableSkipGenerateResidentMips;
	static int32 EnableOnlyGenerateRequestedLODs;
	static int32 MaxTextureSizeToGenerate;
	static bool bEnableMutableReusePreviousUpdateData;

	/** */
	inline void AddGameThreadTask(const FMutableTask& Task)
	{
		check(IsInGameThread())
		PendingTasks.Enqueue(Task);
	}


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


	void AddTextureReference(const FMutableImageCacheKey& TextureId)
	{
		uint32& CountRef = TextureReferenceCount.FindOrAdd(TextureId);

		CountRef++;
	}

	
	// Returns true if the texture's references become zero
	bool RemoveTextureReference(const FMutableImageCacheKey& TextureId)
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


	bool TextureHasReferences(const FMutableImageCacheKey& TextureId) const
	{
		const uint32* CountPtr = TextureReferenceCount.Find(TextureId);

		if (CountPtr && *CountPtr > 0)
		{
			return true;
		}

		return false;
	}


	// Init the async Skeletal Mesh creation/update
	void InitUpdateSkeletalMesh(UCustomizableObjectInstance& Public, EQueuePriorityType Priority, bool bIsCloseDistTick, FInstanceUpdateDelegate* UpdateCallback = nullptr);
		
	// Init an async and safe release of the UE and Mutable resources used by the instance without actually destroying the instance, for example if it's very far away
	void InitDiscardResourcesSkeletalMesh(UCustomizableObjectInstance* InCustomizableObjectInstance);

	// Init the async release of a Mutable Core Instance ID and all the temp resources associated with it
	void InitInstanceIDRelease(mu::Instance::ID IDToRelease);

	void GetMipStreamingConfig(const UCustomizableObjectInstance& Instance, bool& bOutNeverStream, int32& OutMipsToSkip) const;
	
	bool IsReplaceDiscardedWithReferenceMeshEnabled() const { return bReplaceDiscardedWithReferenceMesh; }
	void SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled) { bReplaceDiscardedWithReferenceMesh = bIsEnabled; }

	int32 GetCountAllocatedSkeletalMesh() { return CountAllocatedSkeletalMesh; }

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
	TMap<FMutableImageCacheKey, uint32> TextureReferenceCount; // Keeps a count of texture usage to decide if they have to be blocked from GC during an update

	// This is protected from GC by AddReferencedObjects
	TObjectPtr<UCustomizableObjectInstance> CurrentInstanceBeingUpdated = nullptr;

	TSharedPtr<FMutableOperation> CurrentMutableOperation = nullptr;

	// Handle to the registered TickDelegate.
	FTSTicker::FDelegateHandle TickDelegateHandle;
	FTickerDelegate TickDelegate;

	//! Update the stats logged in unreal's stats system. 
	void UpdateStats();

	// Important!!! Never call when there's a Begin Update thread running!
	void ReleasePendingMutableInstances();

	/** Update the last set amount of internal memory Mutable can use to build objects. */
	void UpdateMemoryLimit();

	bool IsMutableAnimInfoDebuggingEnabled() const;

	FUnrealMutableImageProvider* GetImageProviderChecked() const;
	
	/** Mutable TaskGraph system (Mutable Thread). */
	FMutableTaskGraph MutableTaskGraph;
};

