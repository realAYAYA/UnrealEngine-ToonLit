// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/IConsoleManager.h"
#include "Engine/StreamableManager.h"
#include "AssetRegistry/AssetData.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectInstanceDescriptor.h"
#include "MuR/Parameters.h"
#include "MuR/Image.h"
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
class UMaterialInterface;
class UTexture2D;
class UCustomizableObjectSystemPrivate; // This is used to hide Mutable SDK members in the public headers.
class FUpdateContextPrivate;
struct FFrame;
struct FGuid;


constexpr uint64 KEY_OFFSET_COMPILATION_OUT_OF_DATE = 1;

extern TAutoConsoleVariable<bool> CVarClearWorkingMemoryOnUpdateEnd;

extern TAutoConsoleVariable<bool> CVarReuseImagesBetweenInstances;

extern TAutoConsoleVariable<bool> CVarPreserveUserLODsOnFirstGeneration;

extern TAutoConsoleVariable<bool> CVarEnableMeshCache;

extern TAutoConsoleVariable<bool> CVarRollbackFixModelDiskStreamerDataRace;

extern TAutoConsoleVariable<bool> CVarEnableNewSplitMutableTask;

#if WITH_EDITOR

// Struct used to keep a copy of the EditorSettings needed to compile Customizable Objects.
struct FEditorCompileSettings 
{
	// General case
	bool bIsMutableEnabled = true;

	// Auto Compile 
	bool bEnableAutomaticCompilation = true;
	bool bCompileObjectsSynchronously = true;
	bool bCompileRootObjectsOnStartPIE = false;
};

#endif

namespace EMutableProfileMetric
{
	typedef uint8 Type;

	constexpr Type BuiltInstances = 1;
	constexpr Type UpdateOperations = 2;
	constexpr Type Count = 4;

};


USTRUCT()
struct FPendingReleaseMaterialsInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY()
	int32 TicksUntilRelease = 0;
};



/** Key to identify an image inside a generated mutable runtime instance. */
struct FMutableImageReference
{
	/** Original image ID. Once generated it will be unique. However, future updates of the image may return a different ID for the
	* same image, if many other resources have been built in the middle. For this reason the rest of the data in the struct is what
	* must be used to request the additional mips.	*/
	uint32 ImageID = 0;

	uint32 SurfaceId = 0;

	uint8 LOD = 0;
	uint8 Component = 0;
	uint8 Image = 0;

	uint8 BaseMip = 0;
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
	virtual void GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues) {}
};


UCLASS(Blueprintable, BlueprintType)
class CUSTOMIZABLEOBJECT_API UCustomizableObjectSystem : public UObject
{
	// Friends
	friend class UCustomizableObjectSystemPrivate;

public:
	GENERATED_BODY()

	UCustomizableObjectSystem() = default;
	void InitSystem();

	/** Get the singleton object. It will be created if it doesn't exist yet. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Status)
	static UCustomizableObjectSystem* GetInstance();
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Status)
	static UCustomizableObjectSystem* GetInstanceChecked();

	/** Determines if the result of the instance update is valid or not.
	 * @return true if the result is successful or has warnings, false if the result is from the Error category */
	UFUNCTION(BlueprintCallable, Category = Status)
	static bool IsUpdateResultValid(const EUpdateResult UpdateResult);
	
	// Return true if the singleton has been created. It is different than GetInstance in that GetInstance will create it if it doesn't exist.
	static bool IsCreated();

	/** Returns the current status of Mutable. Only when active is it possible to compile COs, generate instances, and stream textures.
	  * @return True if Mutable is enabled. */
	static bool IsActive();

	// Begin UObject interface.
	virtual void BeginDestroy() override;
	virtual FString GetDesc() override;
	// End UObject interface.

	// Creates a new Customizable Object Compiler (Only does real work in editor builds). The caller is responsible for freeing the new compiler
	FCustomizableObjectCompilerBase* GetNewCompiler();
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
	bool LockObject(const UCustomizableObject*);
	void UnlockObject(const UCustomizableObject*);

	/** Checks if there are any outstanding disk or mip update operations in flight for the parameter Customizable Object that may
	* make it unsafe to compile at the moment.
	* @return true if there are operations in flight and it's not safe to compile */
	bool CheckIfDiskOrMipUpdateOperationsPending(const UCustomizableObject& Object) const;
	
	// Called whenever the Mutable Editor Settings change, copying the new value of the current needed settings to the Customizable Object System
	void EditorSettingsChanged(const FEditorCompileSettings& InEditorSettings);

	// If true, uncompiled Customizable Objects will be compiled whenever an instance update is required
	bool IsAutoCompileEnabled() const;

	/** Return true if inside commandlets uncompiled Customizable Objects will be compiled whenever an instance update is required. */
	bool IsAutoCompileCommandletEnabled() const;

	/** Set if inside commandlets uncompiled Customizable Objects will be compiled whenever an instance update is required. */
	void SetAutoCompileCommandletEnabled(bool bValue);
	
	// If true, uncompiled Customizable Objects will be compiled synchronously
	bool IsAutoCompilationSync() const;
#endif
	
	// Return the current MinLodQualityLevel for skeletal meshes.
	int32 GetSkeletalMeshMinLODQualityLevel() const;

	bool IsSupport16BitBoneIndexEnabled() const;

	bool IsProgressiveMipStreamingEnabled() const;
	void SetProgressiveMipStreamingEnabled(bool bIsEnabled);

	bool IsOnlyGenerateRequestedLODsEnabled() const;
	void SetOnlyGenerateRequestedLODsEnabled(bool bIsEnabled);

#if WITH_EDITOR
	void SetImagePixelFormatOverride(const mu::FImageOperator::FImagePixelFormatFunc&);
#endif

	/** [Texture Parameters] Get a list of all the possible values for external texture parameters according to the various providers registered with RegisterImageProvider. */
	TArray<FCustomizableObjectExternalTexture> GetTextureParameterValues();

	/** [Texture Parameters] Add a new image provider to the CustomizableObject System. This will be queried for when an external image ID is provided to mutable in an Texture Parameter node. */
	void RegisterImageProvider(UCustomizableSystemImageProvider* Provider);

	/** [Texture Parameters] Remove a previously registered provider. */
	void UnregisterImageProvider(UCustomizableSystemImageProvider* Provider);

	/** [Texture Parameters] Interface to actually cache Images in the Mutable system and make them available at run-time.
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

	// Show a warning on-screen and via a notification (if in Editor) and log an error when a CustomizableObject is
	// being used and it's not compiled.  Callers can add additional information to the error log.
	void AddUncompiledCOWarning(const UCustomizableObject& InObject, FString const* OptionalLogInfo = nullptr);

	// Enables the collection of internal Mutable performance data. It has a performance cost.
	void EnableBenchmark();
	// Writes the benchmark results
	void EndBenchmark();

	// Show data about all UCustomizableObjectInstance existing elements
	void LogShowData(bool bFullInfo, bool ShowMaterialInfo) const;

	// Give access to the internal object data.
	UCustomizableObjectSystemPrivate* GetPrivate();
	const UCustomizableObjectSystemPrivate* GetPrivate() const;

	UCustomizableInstanceLODManagementBase* GetInstanceLODManagement() const;

	// Pass a null ptr to reset to the default InstanceLODManagement
	void SetInstanceLODManagement(UCustomizableInstanceLODManagementBase* NewInstanceLODManagement);

	// Find out the version of the plugin
	UFUNCTION(BlueprintCallable, Category = Status)
	FString GetPluginVersion() const;

	// Get the number of instances built and alive.
	UFUNCTION(BlueprintCallable, Category = Status)
	int32 GetNumInstances() const;

	// Get the number of instances waiting to be updated.
	UFUNCTION(BlueprintCallable, Category = Status)
	int32 GetNumPendingInstances() const;

	// Get the total number of instances including built and not built.
	UFUNCTION(BlueprintCallable, Category = Status)
	int32 GetTotalInstances() const;

	// Get the amount of GPU memory in use in bytes for textures generated by mutable.
	UFUNCTION(BlueprintCallable, Category = Status)
	int64 GetTextureMemoryUsed() const;

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

private:
	// Most of the work in this plugin happens here.
	bool Tick(float DeltaTime);

	/** Returns the number of remaining operations. */
	int32 TickInternal();

	// If there is an on-going operation, advance it.
	void AdvanceCurrentOperation();

	void DiscardInstances();
	void ReleaseInstanceIDs();

public:
	/** Return true if the instance is being updated. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CustomizableObjectSystem)
	bool IsUpdating(const UCustomizableObjectInstance* Instance) const;

private:
	// TODO: Can we move this to the editor module?
#if WITH_EDITOR
	// Used to ask the user if they want to recompile uncompiled PIE COs
	void OnPreBeginPIE(const bool bIsSimulatingInEditor);

	void StartNextRecompile();
	void TickRecompileCustomizableObjects();
#endif

public:
	/** Set Mutable's working memory limit (bytes). Mutable will flush internal caches to try to keep its memory consumption below the WorkingMemory (i.e., it is not a hard limit).
	 * The working memory limit will especially reduce the memory required to perform Instance Updates and Texture Streaming.
 	 * Notice that Mutable does not track all its memory (e.g., UObjects memory is no tracked).
	 * This value can also be set using "mutable.WorkingMemory" CVar. */
	void SetWorkingMemory(int32 Bytes);

	/** Get Mutable's working memory limit (bytes). See SetWorkingMemory(int32). */
	int32 GetWorkingMemory() const;

#if WITH_EDITOR
	// Copy of the Mutable Editor Settings tied to CO compilation. They are updated whenever changed
	FEditorCompileSettings EditorSettings;
#endif
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectSystemPrivate> Private = nullptr;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Engine/EngineTypes.h"
#include "PixelFormat.h"
#endif
