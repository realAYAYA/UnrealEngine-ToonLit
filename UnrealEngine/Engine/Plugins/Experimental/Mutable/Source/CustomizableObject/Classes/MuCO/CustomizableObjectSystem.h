// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IConsoleManager.h"
#include "Engine/StreamableManager.h"
#include "AssetRegistry/AssetData.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectInstanceDescriptor.h"
#include "MuR/Parameters.h"
#include "MuR/Types.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#endif

#include "CustomizableObjectSystem.generated.h"

class IConsoleVariable;
class FCustomizableObjectCompilerBase;
class UCustomizableInstanceLODManagementBase;
class ITargetPlatform;
class SNotificationItem;
class UCustomizableObject;
class UDefaultImageProvider;
class USkeletalMesh;
class UTexture2D;
struct FFrame;
struct FGuid;


// Split StreamedBulkData into chunks smaller than MUTABLE_STREAMED_DATA_MAXCHUNKSIZE
#define MUTABLE_STREAMED_DATA_MAXCHUNKSIZE		(512 * 1024 * 1024)

// In case of async file operations, what priority to use
#define MUTABLE_SYSTEM_ASYNC_STREAMING_PRIORITY			EAsyncIOPriorityAndFlags::AIOP_Normal

// This is used to hide Mutable SDK members in the public headers.
class FCustomizableObjectSystemPrivate;


// Mutable stats
DECLARE_STATS_GROUP(TEXT("Mutable"), STATGROUP_Mutable, STATCAT_Advanced);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Created Mutable Skeletal Meshes"), STAT_MutableNumSkeletalMeshes, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Cached Mutable Skeletal Meshes"), STAT_MutableNumCachedSkeletalMeshes, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Allocated Mutable Skeletal Meshes"), STAT_MutableNumAllocatedSkeletalMeshes, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Instances at LOD 0"), STAT_MutableNumInstancesLOD0, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Instances at LOD 1"), STAT_MutableNumInstancesLOD1, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Instances at LOD 2 or more"), STAT_MutableNumInstancesLOD2, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Skeletal Mesh Resource Memory"), STAT_MutableSkeletalMeshResourceMemory, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Created Mutable Textures"), STAT_MutableNumTextures, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Cached Mutable Textures"), STAT_MutableNumCachedTextures, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Allocated Mutable Textures"), STAT_MutableNumAllocatedTextures, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Texture Resource Memory"), STAT_MutableTextureResourceMemory, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Texture Generated Memory"), STAT_MutableTextureGeneratedMemory, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Texture Locked Memory"), STAT_MutableTextureCacheMemory, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Pending Instance Updates"), STAT_MutablePendingInstanceUpdates, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Abandoned Instance Updates"), STAT_MutableAbandonedInstanceUpdates, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Last Instance Build Time"), STAT_MutableInstanceBuildTime, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Avrg Instance Build Time"), STAT_MutableInstanceBuildTimeAvrg, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Streaming Ops"), STAT_MutableStreamingOps, STATGROUP_Mutable, );


extern TAutoConsoleVariable<bool> CVarClearWorkingMemoryOnUpdateEnd;

extern TAutoConsoleVariable<bool> CVarReuseImagesBetweenInstances;


#if WITH_EDITOR

// Struct used to keep a copy of the EditorSettings needed to compile Customizable Objects.
struct FEditorCompileSettings 
{
	// General case
	bool bDisableCompilation;

	// Auto Compile 
	bool bEnableAutomaticCompilation = true;
	bool bCompileObjectsSynchronously = true;
	bool bCompileRootObjectsOnStartPIE = false;
};

#endif

//
namespace EMutableProfileMetric
{
	typedef uint8 Type;

	const Type BuiltInstances = 1;
	const Type UpdateOperations = 2;
	const Type Count = 4;

};

USTRUCT()
struct FPendingReleaseSkeletalMeshInfo
{
public:
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY()
	double TimeStamp = 0.0f;
};



/** Key to identify an image inside a generated mutable runtime instance. */
struct FMutableImageReference
{
	/** Original image ID. Once generated it will be unique. However, future updates of the image may return a different ID for the
	* same image, if many other resources have been built in the middle. For this reason the rest of the data in the struct is what
	* must be used to request the additional mips.
	*/
	uint32 ImageID = 0;

	/** */
	uint32 SurfaceId = 0;

	/** */
	uint8 LOD = 0;
	uint8 Component = 0;
	uint8 Image = 0;
};



// Opaque representation of a possible registered value for texture parameters.
struct FCustomizableObjectExternalTexture
{
	FCustomizableObjectExternalTexture() = default;

	FString Name;
	FName Value;
};


/** Base class for Image provider. */
UCLASS(abstract)
class CUSTOMIZABLEOBJECT_API UCustomizableSystemImageProvider : public UObject
{
public:
	GENERATED_BODY()

	enum class ValueType : uint8
	{
		// This texture is not provided by this provider.
		None,

		// Data will be provided with size and pointer
		Raw,

		// Data will be provided from an unreal texture, loaded in the game thread and kept in memory
		Unreal,

		// Data will be provided from an unreal texture, and will only be loaded when actually needed in the Mutable thread
		Unreal_Deferred,

		// Number of elements of this enum.
		Count
	};

	// Query that Mutable will run to find out if a texture will be provided as an Unreal UTexture2D,
	// or as a raw data blob.
	virtual ValueType HasTextureParameterValue(const FName& ID) { return ValueType::None; }

	// In case IsTextureParameterValueUnreal returns false, this will be used to query the texture size data.
	virtual FIntVector GetTextureParameterValueSize(const FName& ID) { return FIntVector(0, 0, 0); }

	// In case IsTextureParameterValueUnreal returns false, this will be used to query the texture data that must
	// be copied in the preallocated buffer. The pixel format is assumed to be 4-channel RGBA, uint8_t per channel.
	virtual void GetTextureParameterValueData(const FName& ID, uint8* OutData) {}

	// In case IsTextureParameterValueUnreal returns true, this will be used to query the texture.
	virtual UTexture2D* GetTextureParameterValue(const FName& ID) { return nullptr; }

	// Used in the editor to show the list of available options.
	// Only necessary if the images are required in editor previews.
	virtual void GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues) {};
};


// Before the Mutable Queue rework this made sense, but this is no longer the case. Remove this when doing MTBL-1409.
/** End a Customizable Object Instance Update. All code paths of an update have to end here. */
void FinishUpdateGlobal(UCustomizableObjectInstance* Instance, EUpdateResult UpdateResult, FInstanceUpdateDelegate* UpdateCallback, const FDescriptorRuntimeHash InUpdatedHash = FDescriptorRuntimeHash());


UCLASS(Blueprintable, BlueprintType)
class CUSTOMIZABLEOBJECT_API UCustomizableObjectSystem : public UObject
{
public:
	GENERATED_BODY()

	UCustomizableObjectSystem() = default;
	void InitSystem();

	/** Get the singleton object. It will be created if it doesn't exist yet.
	 * @param bCreate Create a system if it does not has been created yet. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Status)
	static UCustomizableObjectSystem* GetInstance();
	
	// Return true if the singleton has been created. It is different than GetInstance in that GetInstance will create it if it doesn't exist.
	static bool IsCreated();

	// Begin UObject interface.
	virtual void BeginDestroy() override;
	virtual FString GetDesc() override;
	// End UObject interface.

	// Creates a new Customizable Object Compiler (Only does real work in editor builds). The caller is responsible for freeing the new compiler
	class FCustomizableObjectCompilerBase* GetNewCompiler();
	void SetNewCompilerFunc(FCustomizableObjectCompilerBase* (*NewCompilerFunc)());

	bool IsReplaceDiscardedWithReferenceMeshEnabled() const;
	void SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled);

	// Unprotect the resources in the instances of this object of being garbage collector
	// while an instance is being built or updated, so that they can be reused.
	void ClearResourceCacheProtected();

#if WITH_EDITOR
	// Lock a CustomizableObjects, preventing the generation or update of any of its instances
	// Will return true if successful, false if it fails to lock because an update is already underway
	// This is usually only used in the editor
	bool LockObject(const class UCustomizableObject*);
	void UnlockObject(const class UCustomizableObject*);

	/** Checks if there are any outstading disk or mip update operations in flight for the parameter Customizable Object that may
	* make it unsafe to compile at the moment.
	* @return true if there are operations in flight and it's not safe to compile */
	bool CheckIfDiskOrMipUpdateOperationsPending(const UCustomizableObject& Object) const;
	
	// Called whenever the Mutable Editor Settings change, copying the new value of the current needed settings to the Customizable Object System
	void EditorSettingsChanged(const FEditorCompileSettings& InEditorSettings);

	// If compilation is disabled, Customizable Objects won't be compiled in the editor
	bool IsCompilationDisabled() const;

	// If true, uncompiled Customizable Objects will be compiled whenever an instance update is required
	bool IsAutoCompileEnabled() const;

	// If true, uncompiled Customizable Objects will be compiled synchronously
	bool IsAutoCompilationSync() const;

	// Copy of the Mutable Editor Settings tied to CO compilation. They are updated whenever changed
	FEditorCompileSettings EditorSettings;

#endif
	bool IsSupport16BitBoneIndexEnabled() const;

	bool IsProgressiveMipStreamingEnabled() const;
	void SetProgressiveMipStreamingEnabled(bool bIsEnabled);

	bool IsOnlyGenerateRequestedLODsEnabled() const;
	void SetOnlyGenerateRequestedLODsEnabled(bool bIsEnabled);

	void AddPendingReleaseSkeletalMesh( USkeletalMesh* SkeletalMesh );

	void PurgePendingReleaseSkeletalMesh();

private:

	UPROPERTY()
	TArray<FPendingReleaseSkeletalMeshInfo> PendingReleaseSkeletalMesh;

public:

	/** [Texture Parameters] Get a list of all the possible values for external texture parameters according to the various providers registered with RegisterImageProvider. */
	TArray<FCustomizableObjectExternalTexture> GetTextureParameterValues();

	/** [Texture Parameters] Add a new image provider to the CustomizableObject System. This will be queried for when an external image ID is provided to mutable in an Texture Parameter node. */
	void RegisterImageProvider(UCustomizableSystemImageProvider* Provider);

	/** [Texture Parameters] Remove a previously registered provider. */
	void UnregisterImageProvider(UCustomizableSystemImageProvider* Provider);

	/** [Texture Parameters] Interface to actually cache Images in the Mutable system and make them availabe at run-time.
		Any cached image has to be registered by an Image provider before caching it.
		Have in mind that once an image has been cached, it will spend memory according to its size, except in the case
		of images of type UCustomizableSystemImageProvider::ValueType::Unreal_Deferred, where only a very small amount of
		memory is used and the real texel data is loaded when needed during an update and then immediately discarded */

	/** [Texture Parameters] Cache an image which has to have been previously registered by an Image provider with the parameter id. */
	void CacheImage(FName ImageId);
	/** [Texture Parameters] Remove an image from the cache. */
	void UnCacheImage(FName ImageId);
	/** [Texture Parameters] Remove all images from the cache. */
	void ClearImageCache();

	/** Initialize (if was not already) and get the default image provider. */
	UDefaultImageProvider& GetOrCreateDefaultImageProvider();

private:
	/** Mutable default image provider. Used by the COIEditor and Instance/Descriptor APIs. */
	UPROPERTY()
	TObjectPtr<UDefaultImageProvider> DefaultImageProvider = nullptr;

public:
    
	// Show a warning on-screen and via a notification (if in Editor) and log an error when a CustomizableObject is
	// being used and it's not compiled.  Callers can add additional information to the error log.
	void AddUncompiledCOWarning(const UCustomizableObject& InObject, FString const* OptionalLogInfo = nullptr);

	// Enables the collection of internal mutabe performance data. It has a performance cost.
	void EnableBenchmark();
	// Writes the benchmark results
	void EndBenchmark();

	// Show data about all UCustomizableObjectInstance existing elements
	void LogShowData(bool bFullInfo, bool ShowMaterialInfo) const;


	// Give access to the internal object data.
	FCustomizableObjectSystemPrivate* GetPrivate();
	const FCustomizableObjectSystemPrivate* GetPrivate() const;

	FCustomizableObjectSystemPrivate* GetPrivateChecked();
	const FCustomizableObjectSystemPrivate* GetPrivateChecked() const;
	
	FStreamableManager& GetStreamableManager() { return StreamableManager; }

	UCustomizableInstanceLODManagementBase* GetInstanceLODManagement() { return CurrentInstanceLODManagement.Get(); }

	// Pass a null ptr to reset to the default InstanceLODManagement
	void SetInstanceLODManagement(UCustomizableInstanceLODManagementBase* NewInstanceLODManagement) 
		{ CurrentInstanceLODManagement = NewInstanceLODManagement ? NewInstanceLODManagement : ToRawPtr(DefaultInstanceLODManagement); }

	// Find out the version of the plugin
	UFUNCTION(BlueprintCallable, Category = Status)
	FString GetPluginVersion() const;

	// Get the number of instances built and alive.
	UFUNCTION(BlueprintCallable, Category = Status)
	int32 GetNumInstances() const;

	// Get the number of instances waiting to be updated.
	UFUNCTION(BlueprintCallable, Category = Status)
	int32 GetNumPendingInstances() const;

	// Get the total number of instances includingbuilt and not built.
	UFUNCTION(BlueprintCallable, Category = Status)
	int32 GetTotalInstances() const;

	// Get the amount of memory in use for textures generated by mutable.
	UFUNCTION(BlueprintCallable, Category = Status)
	int32 GetTextureMemoryUsed() const;

	// Return the average build/update time of an instance in ms.
	UFUNCTION(BlueprintCallable, Category = Status)
	int32 GetAverageBuildTime() const;

	// If set to true, Mutable will release Mutable-generated textures immediately when they are no longer used without waiting for GC
	// IMPORTANT!!! Do NOT keep references to any Mutable generated textures or skeletal meshes if this is enabled,
	// they are owned by Mutable and will be destroyed without notice
	UFUNCTION(BlueprintCallable, Category = Status)
	void SetReleaseMutableTexturesImmediately(bool bReleaseTextures);

	bool IsMutableAnimInfoDebuggingEnabled() const;

#if WITH_EDITOR
	void RecompileCustomizableObjectAsync(const FAssetData& InAssetData, const UCustomizableObject* InObject);
	
	void RecompileCustomizableObjects(const TArray<FAssetData>& InObjects);

	// Get the maximum size a chunk can have on a specific platform. If unspecified return MUTABLE_STREAMED_DATA_MAXCHUNKSIZE
	uint64 GetMaxChunkSizeForPlatform(const ITargetPlatform* InTargetPlatform);
#endif

	void ClearCurrentMutableOperation();

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableInstanceLODManagementBase> DefaultInstanceLODManagement = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableInstanceLODManagementBase> CurrentInstanceLODManagement = nullptr;

	// Array where textures are added temporarily while the mutable thread may want to
	// reused them for some instance under construction.
	UPROPERTY(Transient)
	TArray< TObjectPtr<UTexture2D> > ProtectedCachedTextures;

private:

	TSharedPtr<FCustomizableObjectSystemPrivate> Private = nullptr;

	// For async material loading
	FStreamableManager StreamableManager;

	// Most of the work in this plugin happens here.
	bool Tick(float DeltaTime);

	// If there is an on-going operation, advance it.
	void AdvanceCurrentOperation();

	void DiscardInstances();
	void ReleaseInstanceIDs();

	// TODO: Can we move this to the editor module?
#if WITH_EDITOR

	// Used to ask the user if they want to recompile uncompiled PIE COs
	void OnPreBeginPIE(const bool bIsSimulatingInEditor);

	void StartNextRecompile();
	void TickRecompileCustomizableObjects();

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
	
	// Friends
	friend class FCustomizableObjectSystemPrivate;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/EngineTypes.h"
#include "PixelFormat.h"
#endif
