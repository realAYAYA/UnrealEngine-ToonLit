// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LogBenchmarkUtil.h"
#include "Containers/Queue.h"
#include "MuCO/DescriptorHash.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "MuCO/CustomizableInstanceLODManagement.h"
#include "Containers/Ticker.h"

#include "MuCO/CustomizableObjectInstanceDescriptor.h"
#include "MuR/Mesh.h"
#include "MuR/Parameters.h"
#include "MuR/System.h"
#include "MuR/Image.h"
#include "UObject/GCObject.h"
#include "WorldCollision.h"
#include "Engine/StreamableManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "MuCO/FMutableTaskGraph.h"
#include "AssetRegistry/AssetData.h"
#include "ContentStreaming.h"

#include "Tasks/Task.h"

#include "CustomizableObjectSystemPrivate.generated.h"

#if WITH_EDITORONLY_DATA
class UEditorImageProvider;
#endif

class UCustomizableObjectSystem;
namespace LowLevelTasks { enum class ETaskPriority : int8; }
struct FTexturePlatformData;


// Split StreamedBulkData into chunks smaller than MUTABLE_STREAMED_DATA_MAXCHUNKSIZE
#define MUTABLE_STREAMED_DATA_MAXCHUNKSIZE		(512 * 1024 * 1024)


/** End a Customizable Object Instance Update. All code paths of an update have to end here. */
void FinishUpdateGlobal(const TSharedRef<FUpdateContextPrivate>& Context);


class FMutableUpdateCandidate
{
public:
	// The Instance to possibly update
	UCustomizableObjectInstance* CustomizableObjectInstance;
	EQueuePriorityType Priority = EQueuePriorityType::Med;

	// These are the LODs that would be applied if this candidate is chosen
	int32 MinLOD = 0;

	/** Array of RequestedLODs per component to generate if this candidate is chosen */
	TArray<uint16> RequestedLODLevels;

	FMutableUpdateCandidate(UCustomizableObjectInstance* InCustomizableObjectInstance) : CustomizableObjectInstance(InCustomizableObjectInstance)
	{
		const FCustomizableObjectInstanceDescriptor& Descriptor = InCustomizableObjectInstance->GetDescriptor();
		MinLOD = Descriptor.GetMinLod();
		RequestedLODLevels = Descriptor.GetRequestedLODLevels();
	}

	FMutableUpdateCandidate(const UCustomizableObjectInstance* InCustomizableObjectInstance, const int32 InMinLOD,
		const TArray<uint16>& InRequestedLODLevels) :
		CustomizableObjectInstance(const_cast<UCustomizableObjectInstance*>(InCustomizableObjectInstance)), MinLOD(InMinLOD),
		RequestedLODLevels(InRequestedLODLevels) {}

	bool HasBeenIssued() const;

	void Issue();

	void ApplyLODUpdateParamsToInstance(FUpdateContextPrivate& Context);

private:
	/** If true it means that EnqueueUpdateSkeletalMesh has decided this update should be performed, if false it should be ignored. Just used for consistency checks */
	bool bHasBeenIssued = false;
};


struct FMutablePendingInstanceUpdate
{
	TSharedRef<FUpdateContextPrivate> Context;

	FMutablePendingInstanceUpdate(const TSharedRef<FUpdateContextPrivate>& InContext);

	bool operator==(const FMutablePendingInstanceUpdate& Other) const;

	bool operator<(const FMutablePendingInstanceUpdate& Other) const;
};


inline uint32 GetTypeHash(const FMutablePendingInstanceUpdate& Update);


struct FPendingInstanceUpdateKeyFuncs : BaseKeyFuncs<FMutablePendingInstanceUpdate, TWeakObjectPtr<const UCustomizableObjectInstance>>
{
	FORCEINLINE static TWeakObjectPtr<const UCustomizableObjectInstance> GetSetKey(const FMutablePendingInstanceUpdate& PendingUpdate);

	FORCEINLINE static bool Matches(const TWeakObjectPtr<const UCustomizableObjectInstance>& A, const TWeakObjectPtr<const UCustomizableObjectInstance>& B);

	FORCEINLINE static uint32 GetKeyHash(const TWeakObjectPtr<const UCustomizableObjectInstance>& Identifier);
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
	// Returns the number of pending instance updates, LOD Updates, discards and releases last tick.
	int32 Num() const;

	void SetLODUpdatesLastTick(int32 NumLODUpdates);

	// Adds a new instance update
	void AddUpdate(const FMutablePendingInstanceUpdate& UpdateToAdd);

	// Removes an instance update
	void RemoveUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance);

#if WITH_EDITOR
	void RemoveUpdatesForObject(const UCustomizableObject* InObject);
#endif

	const FMutablePendingInstanceUpdate* GetUpdate(const TWeakObjectPtr<const UCustomizableObjectInstance>& Instance) const;

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


USTRUCT()
struct FGeneratedTexture
{
	GENERATED_USTRUCT_BODY();

	FMutableImageCacheKey Key;

	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	FString Name;

	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	TObjectPtr<UTexture> Texture = nullptr;

	bool operator==(const FGeneratedTexture& Other) const = default;
};


USTRUCT()
struct FGeneratedMaterial
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	TObjectPtr<UMaterialInterface> MaterialInterface;

	UPROPERTY(Category = NoCategory, VisibleAnywhere)
	TArray< FGeneratedTexture > Textures;

	// Surface or SharedSurface Id
	uint32 SurfaceId = 0;

	// Index of the material to instantiate (UCustomizableObject::ReferencedMaterials)
	uint32 MaterialIndex = 0;

	bool operator==(const FGeneratedMaterial& Other) const { return SurfaceId == Other.SurfaceId && MaterialIndex == Other.MaterialIndex; };
};


DECLARE_DELEGATE(FMutableTaskDelegate);
struct FMutableTask
{
	/** Actual function to perform in this task. */
	FMutableTaskDelegate Function;

	TArray<UE::Tasks::FTask, TFixedAllocator<2>> Dependencies; 
	
	/** Check if all the dependencies of this task have been completed. */
	FORCEINLINE bool AreDependenciesComplete() const
	{
		for (const UE::Tasks::FTask& Dependency : Dependencies)
		{
			if (!Dependency.IsCompleted())
			{
				return false;
			}
		}

		return true;
	}

	/** Free the handles for any dependency of this task. */
	FORCEINLINE void ClearDependencies()
	{
		Dependencies.Empty();
	}
};


// Mutable data generated during the update steps.
// We keep it from begin to end update, and it is used in several steps.
struct FInstanceUpdateData
{
	struct FImage
	{
		FName Name;
		mu::FResourceID ImageID;
		
		// LOD of the ImageId. If the texture is shared between LOD, first LOD where this image can be found. 
		int32 BaseLOD;
		int32 BaseMip;
		
		uint16 FullImageSizeX, FullImageSizeY;
		mu::Ptr<const mu::Image> Image;
		TWeakObjectPtr<UTexture2D> Cached;

		bool bIsPassThrough = false;
	};

	struct FVector
	{
		FName Name;
		FLinearColor Vector;
	};

	struct FScalar
	{
		FName Name;
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
		
		// True if the Mesh is valid
		bool bGenerated = false;

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
	
	TMap<uint32, TArray<FMorphTargetVertexData>> MorphTargetsVertexData;

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


/** Update Context.
 *
 * Alive from the start to the end of the update (both API and LOD update). */
class FUpdateContextPrivate : public FGCObject
{
public:
	FUpdateContextPrivate(UCustomizableObjectInstance& InInstance, const FCustomizableObjectInstanceDescriptor& Descriptor);

	FUpdateContextPrivate(UCustomizableObjectInstance& InInstance);
	virtual ~FUpdateContextPrivate() override;

	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	int32 GetMinLOD() const;

	void SetMinLOD(int32 MinLOD);

	const TArray<uint16>& GetRequestedLODs() const;

	void SetRequestedLODs(TArray<uint16>& RequestedLODs);

	const FCustomizableObjectInstanceDescriptor& GetCapturedDescriptor() const;

	const FDescriptorHash& GetCapturedDescriptorHash() const;

	const FCustomizableObjectInstanceDescriptor&& MoveCommittedDescriptor();
	
	EQueuePriorityType PriorityType = EQueuePriorityType::Low;
	
	FInstanceUpdateDelegate UpdateCallback;
	FInstanceUpdateNativeDelegate UpdateNativeCallback;

	/** Weak reference to the instance we are operating on.
	 *It is weak because we don't want to lock it in case it becomes irrelevant in the game while operations are pending and it needs to be destroyed. */
	TWeakObjectPtr<UCustomizableObjectInstance> Instance;

private:
	/** Descriptor which the update will be performed on. */
	FCustomizableObjectInstanceDescriptor CapturedDescriptor;

	/** Hash of the descriptor. */
	FDescriptorHash CapturedDescriptorHash;

public:
	/** Instance parameters at the time of the operation request. */
	mu::ParametersPtr Parameters; 
	mu::Ptr<mu::System> MutableSystem;

	bool bOnlyUpdateIfNotGenerated = false;
	bool bIgnoreCloseDist = false;
	bool bForceHighPriority = false;
	
	FInstanceUpdateData InstanceUpdateData;
	TArray<int32> RelevantParametersInProgress;

	TArray<FString> LowPriorityTextures;

	/** This option comes from the operation request */
	bool bNeverStream = false;
	
	/** When this option is enabled it will reuse the Mutable core instance and its temp data between updates.  */
	bool bLiveUpdateMode = false;
	bool bReuseInstanceTextures = false;
	bool bUseMeshCache = false;
	
	/** Whether the mesh to generate should support Mesh LOD streaming or not. */
	bool bStreamMeshLODs = false;

	/** This option comes from the operation request. It is used to reduce the number of mipmaps that mutable must generate for images.  */
	int32 MipsToSkip = 0;

	mu::Instance::ID InstanceID = 0; // Redundant
	const mu::Instance* MutableInstance = nullptr;

	uint8 NumComponents = 0;
	uint8 NumLODsAvailable = 0;
	uint8 FirstLODAvailable = 0;
	uint8 FirstResidentLOD = 0;

	TMap<uint32, FTexturePlatformData*> ImageToPlatformDataMap;

	EUpdateResult UpdateResult = EUpdateResult::Success;

	mu::FImageOperator::FImagePixelFormatFunc PixelFormatOverride;

	/** Mutable Meshes required for each component. Outermost index is the component, inner index is the LOD. */
	TArray<TArray<mu::FResourceID>> MeshDescriptors;

	/** Used to know if the updated instances' meshes are different from the previous ones. 
	  * The index of the array is the component's index.
	  * @return true if the mesh is new or new to this instance (e.g. mesh cached by another instance). */
	TArray<bool> MeshChanged;
	
	bool UpdateStarted = false;
	bool bLevelBegunPlay = false;

	// Update stats
	double StartQueueTime = 0.0;
	double QueueTime = 0.0;
	
	double StartUpdateTime = 0.0;
	double UpdateTime = 0.0;

	double TaskGetMeshTime = 0.0;
	double TaskLockCacheTime = 0.0;
	double TaskGetImagesTime = 0.0;
	double TaskConvertResourcesTime = 0.0f;
	double TaskCallbacksTime = 0.0;

	// Update Memory stats
	int64 UpdateStartBytes = 0;
	int64 UpdateEndPeakBytes = 0;
	int64 UpdateEndRealPeakBytes = 0;
	
	/** Used for profiling in the editor. */
	uint32 MutableRuntimeCycles = 0;

	/** Hard references to objects. Avoids GC to collect them. */
	TArray<TObjectPtr<const UObject>> Objects;
};


/** Runtime data used during a mutable instance update */
struct FMutableReleasePlatformOperationData
{
	TMap<uint32, FTexturePlatformData*> ImageToPlatformDataMap;
};


USTRUCT()
struct FPendingReleaseSkeletalMeshInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY()
	double TimeStamp = 0.0f;
};


UCLASS()
class UCustomizableObjectSystemPrivate : public UObject, public IStreamingManager
{
	GENERATED_BODY()
	
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

	// Queue of game-thread tasks that need to be executed for the current operation. TQueue is thread safe
	TQueue<FMutableTask> PendingTasks;

	static int32 EnableMutableProgressiveMipStreaming;
	static int32 EnableMutableLiveUpdate;
	static int32 EnableReuseInstanceTextures;
	static int32 EnableMutableAnimInfoDebugging;
	static int32 EnableSkipGenerateResidentMips;
	static int32 EnableOnlyGenerateRequestedLODs;
	static int32 MaxTextureSizeToGenerate;
	static int32 SkeletalMeshMinLodQualityLevel;

#if WITH_EDITOR
	mu::FImageOperator::FImagePixelFormatFunc ImageFormatOverrideFunc;
#endif

	// IStreamingManager interface
	virtual void UpdateResourceStreaming(float DeltaTime, bool bProcessEverything = false) override;
	virtual int32 BlockTillAllRequestsFinished(float TimeLimit = 0.0f, bool bLogResults = false) override;
	virtual void CancelForcedResources() override {}
	virtual void AddLevel(ULevel* Level) override {}
	virtual void RemoveLevel(ULevel* Level) override {}
	virtual void NotifyLevelChange() override {}
	virtual void SetDisregardWorldResourcesForFrames(int32 NumFrames) override {}
	virtual void NotifyLevelOffset(ULevel* Level, const FVector& Offset) override {}

	UCustomizableObjectSystem* GetPublic() const;
	
	void AddGameThreadTask(const FMutableTask& Task);

	// Remove references to cached objects that have been deleted in the unreal
	// side, and cannot be cached anyway.
	// This should only happen in the game thread
	void CleanupCache();

	// This should only happen in the game thread
	FMutableResourceCache& GetObjectCache(const UCustomizableObject* Object);

	void AddTextureReference(const FMutableImageCacheKey& TextureId);

	// Returns true if the texture's references become zero
	bool RemoveTextureReference(const FMutableImageCacheKey& TextureId);

	bool TextureHasReferences(const FMutableImageCacheKey& TextureId) const;

	EUpdateRequired IsUpdateRequired(const UCustomizableObjectInstance& Instance, bool bOnlyUpdateIfNotGenerated, bool bOnlyUpdateIfLOD, bool bIgnoreCloseDist) const;

	EQueuePriorityType GetUpdatePriority(const UCustomizableObjectInstance& Instance, bool bForceHighPriority) const;

	void EnqueueUpdateSkeletalMesh(const TSharedRef<FUpdateContextPrivate>& Context);
		
	// Init an async and safe release of the UE and Mutable resources used by the instance without actually destroying the instance, for example if it's very far away
	void InitDiscardResourcesSkeletalMesh(UCustomizableObjectInstance* InCustomizableObjectInstance);

	// Init the async release of a Mutable Core Instance ID and all the temp resources associated with it
	void InitInstanceIDRelease(mu::Instance::ID IDToRelease);

	void GetMipStreamingConfig(const UCustomizableObjectInstance& Instance, bool& bOutNeverStream, int32& OutMipsToSkip) const;
	
	bool IsReplaceDiscardedWithReferenceMeshEnabled() const;
	void SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled);

	/** Updated at the beginning of each tick. */
	int32 GetNumSkeletalMeshes() const;

	bool bReplaceDiscardedWithReferenceMesh = false;
	bool bReleaseTexturesImmediately = false;

	bool bSupport16BitBoneIndex = false;

	static FCustomizableObjectCompilerBase* (*NewCompilerFunc)();

	TMap<FMutableImageCacheKey, uint32> TextureReferenceCount; // Keeps a count of texture usage to decide if they have to be blocked from GC during an update

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectInstance> CurrentInstanceBeingUpdated = nullptr;

	TSharedPtr<FUpdateContextPrivate> CurrentMutableOperation = nullptr;

	// Handle to the registered TickDelegate.
	FTSTicker::FDelegateHandle TickDelegateHandle;
	FTickerDelegate TickDelegate;

	/** Change the current status of Mutable. Enabling/Disabling core features.	
	 * Disabling Mutable will turn off compilation, generation, and streaming and will remove the system ticker. */
	static void OnMutableEnabledChanged(IConsoleVariable* CVar = nullptr);

	/** Update the last set amount of internal memory Mutable can use to build objects. */
	void UpdateMemoryLimit();

	bool IsMutableAnimInfoDebuggingEnabled() const;

	FUnrealMutableImageProvider* GetImageProviderChecked() const;

	/** Start the actual work of Update Skeletal Mesh process (Update Skeletal Mesh without the queue). */
	void StartUpdateSkeletalMesh(const TSharedRef<FUpdateContextPrivate>& Context);

	/** See UCustomizableObjectInstance::IsUpdating. */
	bool IsUpdating(const UCustomizableObjectInstance& Instance) const;

	/** Update stats at each tick.
	 * Used for stats that are costly to update. */
	void UpdateStats();

	void CacheTextureParameters(const TArray<FCustomizableObjectTextureParameterValue>& TextureParameters) const;

	void UnCacheTextureParameters(const TArray<FCustomizableObjectTextureParameterValue>& TextureParameters) const;

	/** Tick, in the Game Thread, anything that could potentially block the Mutable Thread. */
	void TickMutableThreadDependencies();

	/** Mutable TaskGraph system (Mutable Thread). */
	FMutableTaskGraph MutableTaskGraph;
	
#if WITH_EDITORONLY_DATA
	/** Mutable default image provider. Used by the COIEditor and Instance/Descriptor APIs. */
	UPROPERTY(Transient)
	TObjectPtr<UEditorImageProvider> EditorImageProvider = nullptr;
#endif

	FLogBenchmarkUtil LogBenchmarkUtil;

	int32 NumSkeletalMeshes = 0;

	bool bAutoCompileCommandletEnabled = false;

	UPROPERTY()
	TArray<FPendingReleaseSkeletalMeshInfo> PendingReleaseSkeletalMesh;
	
	UPROPERTY(Transient)
	TObjectPtr<UCustomizableInstanceLODManagementBase> DefaultInstanceLODManagement = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableInstanceLODManagementBase> CurrentInstanceLODManagement = nullptr;

	// Array where textures are added temporarily while the mutable thread may want to
	// reused them for some instance under construction.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> ProtectedCachedTextures;
	
	// For async material loading
	FStreamableManager StreamableManager;
	
#if WITH_EDITOR
	FCustomizableObjectCompilerBase* RecompileCustomizableObjectsCompiler = nullptr;
	
	TArray<FAssetData> ObjectsToRecompile;
	uint32 TotalNumObjectsToRecompile = 0;
	uint32 NumObjectsCompiled = 0;

	/** Recompile progress bar handle */
	FProgressNotificationHandle RecompileNotificationHandle;

	// Array to keep track of cached objects
	TArray<FGuid> UncompiledCustomizableObjectIds;

	/** Weak pointer to the Uncompiled Customizable Objects notification */
	TWeakPtr<SNotificationItem> UncompiledCustomizableObjectsNotificationPtr;

	/** Map used to cache per platform MaxChunkSize. If MaxChunkSize > 0, streamed data will be split in multiple files */
	TMap<FString, int64> PlatformMaxChunkSize;
#endif
};


namespace impl
{
	void CreateMutableInstance(const TSharedRef<FUpdateContextPrivate>& Operation);

	void FixLODs(const TSharedRef<FUpdateContextPrivate>& Operation);
	
	void Subtask_Mutable_PrepareSkeletonData(const TSharedRef<FUpdateContextPrivate>& OperationData);

	void Subtask_Mutable_UpdateParameterRelevancy(const TSharedRef<FUpdateContextPrivate>& OperationData);
	
	void Subtask_Mutable_PrepareTextures(const TSharedRef<FUpdateContextPrivate>& OperationData);
}


/** Set OnlyLOD to -1 to generate all mips */
CUSTOMIZABLEOBJECT_API FTexturePlatformData* MutableCreateImagePlatformData(mu::Ptr<const mu::Image> MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY);
