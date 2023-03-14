// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Engine/EngineTypes.h"
#include "Engine/StreamableManager.h"
#include "Math/IntVector.h"
#include "Math/UnrealMathSSE.h"
#include "PixelFormat.h"
#include "Stats/Stats2.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "AssetRegistry/AssetData.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#endif

#include "CustomizableObjectSystem.generated.h"

class FCustomizableObjectCompilerBase;
class ITargetPlatform;
class SNotificationItem;
class UCustomizableObject;
class USkeletalMesh;
class UTexture2D;
struct FFrame;
struct FGuid;

// This sets the amount of memory in bytes used to keep streaming data after using it, to reduce the streaming load.
// High values use more memory, but save object construction time.
// Setting it to 0 was the original behaviour, and keeps all the data loaded for a mutable operation until the
// next operation.
// This can be overwritten with the console variable b.MutableStreamingMemory
#if !PLATFORM_DESKTOP
	#define MUTABLE_STREAMING_CACHE				(3 * 1024 * 1024)
#else
	#define MUTABLE_STREAMING_CACHE				(12 * 1024 * 1024)
#endif

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
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Texture Parameter Decoration Memory"), STAT_MutableTextureParameterDecorationMemory, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Pending Instance Updates"), STAT_MutablePendingInstanceUpdates, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Abandoned Instance Updates"), STAT_MutableAbandonedInstanceUpdates, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Last Instance Build Time"), STAT_MutableInstanceBuildTime, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Avrg Instance Build Time"), STAT_MutableInstanceBuildTimeAvrg, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Streaming Ops"), STAT_MutableStreamingOps, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Streaming Cache"), STAT_MutableStreamingCache, STATGROUP_Mutable, );

// These stats are provided by the mutable runtime
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Profile-LiveInstanceCount"), STAT_MutableProfile_LiveInstanceCount, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Profile-StreamingCacheBytes"), STAT_MutableProfile_StreamingCacheBytes, STATGROUP_Mutable, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Profile-InstanceUpdateCount"), STAT_MutableProfile_InstanceUpdateCount, STATGROUP_Mutable, );


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
	int64_t Value = 0;
};


//
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
		Unreal_Deferred
	};

	// Query that Mutable will run to find out if a texture will be provided as an Unreal UTexture2D,
	// or as a raw data blob.
	virtual ValueType HasTextureParameterValue(int64 ID) { return ValueType::None; }

	// In case IsTextureParameterValueUnreal returns false, this will be used to query the texture size data.
	virtual FIntVector GetTextureParameterValueSize(int64 ID) { return FIntVector(0, 0, 0); }

	// In case IsTextureParameterValueUnreal returns false, this will be used to query the texture data that must
	// be copied in the preallocated buffer. The pixel format is assumed to be 4-channel RGBA, uint8_t per channel.
	virtual void GetTextureParameterValueData(int64 ID, uint8* OutData) {}

	// In case IsTextureParameterValueUnreal returns true, this will be used to query the texture.
	virtual UTexture2D* GetTextureParameterValue(int64 ID) { return nullptr; }

	// Used in the editor to show the list of available options.
	// Only necessary if the images are required in editor previews.
	virtual void GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues) {};
};


// Example implementation of a ICustomizableSystemImageProvider that just uses a predefined array of textures.
// It is also used by editors to set some preview images.
UCLASS()
class CUSTOMIZABLEOBJECT_API UCustomizableObjectImageProviderArray : public UCustomizableSystemImageProvider
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Preview)
	TArray< TObjectPtr<UTexture2D> > Textures;

	// UObject interface
#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// UCustomizableSystemImageProvider interface
	ValueType HasTextureParameterValue(int64 ID) override;
	UTexture2D* GetTextureParameterValue(int64 ID) override;
	void GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues) override;

	// Own interface
	void InvalidateIds();

	DECLARE_MULTICAST_DELEGATE(FOnTexturesChanged);
	FOnTexturesChanged TexturesChangeDelegate;

private:

	// The preview texture values will start at this ID, to avoid clashing with other game-specific custom providers.
	int FirstId = 100000;
};


class UCustomizableInstanceLODManagementBase;


UCLASS(Blueprintable, BlueprintType)
class CUSTOMIZABLEOBJECT_API UCustomizableObjectSystem : public UObject
{
public:
	GENERATED_BODY()

	UCustomizableObjectSystem() = default;
	void InitSystem();

	// Get the singleton object. It will be created if it doesn't exist yet.
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

	bool IsCompactSerializationEnabled() const;

	bool IsSupport16BitBoneIndexEnabled() const;

	bool IsProgressiveMipStreamingEnabled() const;

public:

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
	void CacheImage(uint64 ImageId);
	/** [Texture Parameters] Remove an image from the cache. */
	void UnCacheImage(uint64 ImageId);
	/** [Texture Parameters] Cache all images which have been previously registered by all registered Image provider with the parameter id
		that was used in the provider. If bClearPreviousCacheImages is true, then all previous cache state is cleared */
	void CacheAllImagesInAllProviders(bool bClearPreviousCacheImages);
	/** [Texture Parameters] Remove all images from the cache. */
	void ClearImageCache();


#if WITH_EDITOR
	/** [Texture Parameters] Get the editor-side preview external texture provider. 
	 * \TODO: Move to the editor? 
	 */
	UCustomizableObjectImageProviderArray* GetEditorExternalImageProvider();
#endif

private:

	/** [Texture Parameters] If in editor, this will hold a reference to the image provider used to give examples of external images to preview in the customizable objects that use this functionality.
	 * \TODO : Move to the editor ?
	 */
	UPROPERTY()
	TObjectPtr<UCustomizableObjectImageProviderArray> PreviewExternalImageProvider = nullptr;

public:
    
	// Show a warning when a CustomizableObject is being used and it's not compiled 
	void AddUncompiledCOWarning(UCustomizableObject* InObject);

	// Enables the collection of internal mutabe performance data. It has a performance cost.
	void EnableBenchmark();
	// Writes the benchmark results
	void EndBenchmark();

	// Show data about all UCustomizableObjectInstance existing elements
	void LogShowData(bool bFullInfo, bool ShowMaterialInfo) const;


	// Give access to the internal object data.
	FCustomizableObjectSystemPrivate* GetPrivate() { return Private.Get(); }

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
	bool bMaterialsAlreadyLoaded = false;

	// Most of the work in this plugin happens here.
	bool Tick(float DeltaTime);

	// If there is an on-going operation, advance it.
	void AdvanceCurrentOperation();

	FTimerHandle TimerHandle_ApplyRandomValues;

	bool ApplyRandomValuesToAllPawns = false;

	bool RandomValuesCommandTimerWorking = false;

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
};

