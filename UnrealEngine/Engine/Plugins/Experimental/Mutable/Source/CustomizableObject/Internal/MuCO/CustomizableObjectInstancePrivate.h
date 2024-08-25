// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuR/Instance.h"
#include "GameplayTagContainer.h"
#include "MuCO/DescriptorHash.h"
#include "UObject/Package.h"

#include "CustomizableObjectInstancePrivate.generated.h"

namespace mu 
{
	class PhysicsBody;
	class Mesh;
	typedef uint64 FResourceID;
}

struct FModelResources;
struct FMutableModelImageProperties;
struct FMutableRefSkeletalMeshData;
struct FMutableImageCacheKey;
struct FStreamableManager;
struct FStreamableHandle;
struct FGeneratedMaterial;
struct FGeneratedTexture;
class UPhysicsAsset;
class USkeleton;


/** \param OnlyLOD: If not 0, extract and convert only one single LOD from the source image.
  * \param ExtractChannel: If different than -1, extract a single-channel image with the specified source channel data. */
CUSTOMIZABLEOBJECT_API void ConvertImage(UTexture2D* Texture, mu::Ptr<const mu::Image> MutableImage, const FMutableModelImageProperties& Props, int32 OnlyLOD = -1, int32 ExtractChannel = -1);


/** CustomizableObject Instance flags for internal use  */
enum ECOInstanceFlags
{
	ECONone							= 0,  // Should not use the name None here.. it collides with other enum in global namespace

	// Update process
	ReuseTextures					= 1 << 3, 	// 
	ReplacePhysicsAssets			= 1 << 4,	// Merge active PhysicsAssets and replace the base physics asset

	// Update priorities
	UsedByComponent					= 1 << 5,	// If any components are using this instance, they will set flag every frame
	UsedByComponentInPlay			= 1 << 6,	// If any components are using this instance in play, they will set flag every frame
	UsedByPlayerOrNearIt			= 1 << 7,	// The instance is used by the player or is near the player, used to give more priority to its updates
	DiscardedByNumInstancesLimit	= 1 << 8,	// The instance is descarded because we exceeded the limit of instances generated 

	// Types of updates
	PendingLODsUpdate				= 1 << 9,	// Used to queue an update due to a change in LODs required by the instance
	PendingLODsDowngrade			= 1 << 10,	// Used to queue a downgrade update to reduce the number of LODs. LOD update goes from a high res level to a low res one, ex: 0 to 1 or 1 to 2
	
	// Generation
	ForceGenerateMipTail			= 1 << 13,	// If set, SkipGenerateResidentMips will be ignored and the mip tail will be generated
};

ENUM_CLASS_FLAGS(ECOInstanceFlags);


USTRUCT()
struct FReferencedPhysicsAssets
{
	GENERATED_USTRUCT_BODY();
	
	TArray<int32> PhysicsAssetToLoad;
	
	UPROPERTY(Transient)
	TArray< TObjectPtr<UPhysicsAsset> > PhysicsAssetsToMerge;

	TArray<int32> AdditionalPhysicsAssetsToLoad;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPhysicsAsset>> AdditionalPhysicsAssets;
};


USTRUCT()
struct FReferencedSkeletons
{
	GENERATED_USTRUCT_BODY();

	// Merged skeleton if found in the cache
	UPROPERTY()
	TObjectPtr<USkeleton> Skeleton;

	UPROPERTY()
	TArray<uint16> SkeletonIds;

	UPROPERTY()
	TArray< TObjectPtr<USkeleton> > SkeletonsToMerge;
};


USTRUCT()
struct FCustomizableInstanceComponentData
{
	GENERATED_USTRUCT_BODY();

	// AnimBP data gathered for a component from its constituent meshes
	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	TMap<FName, TSoftClassPtr<UAnimInstance>> AnimSlotToBP;

	// AssetUserData gathered for a component from its constituent meshes
	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	TSet<TObjectPtr<UAssetUserData>> AssetUserDataArray;

	// Index of the resource in the StreamedResourceData array of the CustomizableObject.
	TArray<int32> StreamedResourceIndex;

#if WITH_EDITORONLY_DATA
	// Just used for mutable.EnableMutableAnimInfoDebugging command
	TArray<FString> MeshPartPaths;
#endif

	/** Skeletons required by the current generated instance. Skeletons to be loaded and merged.*/
	UPROPERTY(Transient)
	FReferencedSkeletons Skeletons;
	
	/** PhysicsAssets required by the current generated instance. PhysicsAssets to be loaded and merged.*/
	UPROPERTY(Transient)
	FReferencedPhysicsAssets PhysicsAssets;

	/** Clothing PhysicsAssets required by the current generated instance. PhysicsAssets to be loaded and merged.*/
	TArray<TPair<int32, int32>> ClothingPhysicsAssetsToStream;

	/** Array of generated MeshIds per each LOD, used to decide if the mesh should be updated or not.
	 *  Size == NumLODsAvailable
	 *  LODs without mesh will be set to the maximum value of FResourceID (Max_uint64). */
	TArray<mu::FResourceID> LastMeshIdPerLOD;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
};

USTRUCT()
struct FAnimInstanceOverridePhysicsAsset
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	int32 PropertyIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsAsset> PhysicsAsset;
};

USTRUCT()
struct FAnimBpGeneratedPhysicsAssets
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<FAnimInstanceOverridePhysicsAsset> AnimInstancePropertyIndexAndPhysicsAssets;
};


UCLASS()
class CUSTOMIZABLEOBJECT_API UCustomizableInstancePrivate : public UObject
{
public:
	GENERATED_BODY()

	/** The generated skeletal meshes for this Instance, one for each component */
	UPROPERTY(Transient, VisibleAnywhere, Category = NoCategory)
	TArray<TObjectPtr<USkeletalMesh>> SkeletalMeshes;

	UPROPERTY(Transient, VisibleAnywhere, Category = NoCategory)
	TArray<FGeneratedMaterial> GeneratedMaterials;

	UPROPERTY( Transient )
	TArray<FGeneratedTexture> GeneratedTextures;

	// Indices of the parameters that are relevant for the given parameter values.
	// This only gets updated if parameter decorations are generated.
	TArray<int> RelevantParameters;

	// If Texture reuse is enabled, stores which texture is being used in a particular <LODIndex, ComponentIndex, MeshSurfaceIndex, image>
	// \TODO: Create a key based on a struct instead of generating strings dynamically.
	UPROPERTY(Transient)
	TMap<FString, TWeakObjectPtr<UTexture2D>> TextureReuseCache;

	// Handle used to store a streaming request operation if one is ongoing.
	TSharedPtr<FStreamableHandle> StreamingHandle;

	// Only used in LiveUpdateMode to reuse core instances between updates and their temp data to speed up updates, but spend way more memory
	mu::Instance::ID LiveUpdateModeInstanceID = 0;

#if WITH_EDITOR

	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	void BindObjectDelegates(UCustomizableObject* CurrentCustomizableObject, UCustomizableObject* NewCustomizableObject);

	void OnPostCompile();
	void OnObjectStatusChanged(FCustomizableObjectStatus::EState Previous, FCustomizableObjectStatus::EState Next);
#endif

	/** Invalidates the previously generated data and retrieves information from the CObject after specific actions.
	 *  It'll be called in the PostLoad, after Compiling the CO, and after changing the CO of the Instance. */
	void InitCustomizableObjectData(const UCustomizableObject* InCustomizableObject);
	
	FCustomizableInstanceComponentData* GetComponentData(int32 ComponentIndex);
	const FCustomizableInstanceComponentData* GetComponentData(int32 ComponentIndex) const;

	ECOInstanceFlags GetCOInstanceFlags() const { return InstanceFlagsPrivate; }
	void SetCOInstanceFlags(ECOInstanceFlags FlagsToSet) { InstanceFlagsPrivate = (ECOInstanceFlags)(InstanceFlagsPrivate | FlagsToSet); }
	void ClearCOInstanceFlags(ECOInstanceFlags FlagsToClear) { InstanceFlagsPrivate = (ECOInstanceFlags)(InstanceFlagsPrivate & ~FlagsToClear); }
	bool HasCOInstanceFlags(ECOInstanceFlags FlagsToCheck) const { return (InstanceFlagsPrivate & FlagsToCheck) != 0; }

	void BuildMaterials(const TSharedRef<FUpdateContextPrivate>& OperationData, UCustomizableObjectInstance* Public);

	void ReuseTexture(UTexture2D* Texture, TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe>& PlatformData);

	/** Returns the task that will be called when all assets and data are loaded, may be already completed if no assets or data needs loading */
	UE::Tasks::FTask LoadAdditionalAssetsAndDataAsync(const TSharedRef<FUpdateContextPrivate>& OperationData, FStreamableManager &StreamableManager);

	void AdditionalAssetsAsyncLoaded(UCustomizableObjectInstance* Public);

	void TickUpdateCloseCustomizableObjects(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates);
	void UpdateInstanceIfNotGenerated(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates);

	// Returns true if success (?)
	bool UpdateSkeletalMesh_PostBeginUpdate0(UCustomizableObjectInstance* Public, const TSharedRef<FUpdateContextPrivate>& OperationData);

	static void ReleaseMutableTexture(const FMutableImageCacheKey& MutableTextureKey, UTexture2D* Texture, struct FMutableResourceCache& Cache);

	// Copy data generated in the mutable thread over to the instance and initializes additional data required during the update
	void PrepareForUpdate(const TSharedRef<FUpdateContextPrivate>& OperationData);

	int32 GetNumLODsAvailable() const { return NumLODsAvailable; }
	
	// The following method is basically copied from PostEditChangeProperty and/or SkeletalMesh.cpp to be able to replicate PostEditChangeProperty without the editor
	void PostEditChangePropertyWithoutEditor();
	
	/** Calls ReleaseResources on all SkeletalMeshes generated by this instance and invalidates the generated data.
	  * It should not be called if the meshes are still in use or shared with other instances. */
	void DiscardResources();

	// Releases all the mutable resources this instance holds, should only be called when it is not going to be used any more.
	void ReleaseMutableResources(bool bCalledFromBeginDestroy, const UCustomizableObjectInstance& Instance);

	/** Set the reference SkeletalMesh, or an empty mesh, to all actors using this instance. */
	void SetDefaultSkeletalMesh(bool bSetEmptyMesh = false) const;

	const TArray<FAnimInstanceOverridePhysicsAsset>* GetGeneratedPhysicsAssetsForAnimInstance(TSubclassOf<UAnimInstance> AnimInstance) const;

	/** */
#if WITH_EDITORONLY_DATA
	void RegenerateImportedModels();
#endif

private:

	void InitSkeletalMeshData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, const UCustomizableObject& CustomizableObject, int32 ComponentIndex);

	bool BuildSkeletonData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh& SkeletalMesh, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, UCustomizableObject& CustomizableObject, int32 ComponentIndex);
	void BuildMeshSockets(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const FModelResources& ModelResources, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, mu::Ptr<const mu::Mesh> MutableMesh);
	void BuildOrCopyElementData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	void BuildOrCopyMorphTargetsData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	bool BuildOrCopyRenderData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	void BuildOrCopyClothingData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	
	//
	USkeleton* MergeSkeletons(UCustomizableObject& CustomizableObject, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, int32 ComponentIndex);

	//
	UPhysicsAsset* GetOrBuildMainPhysicsAsset(TObjectPtr<class UPhysicsAsset> TamplateAsset, const mu::PhysicsBody* PhysicsBody, const UCustomizableObject& CustomizableObject, int32 ComponentIndex, bool bDisableCollisionBetweenAssets);
	
	// Create a transient texture and add it to the TextureTrackerArray
	UTexture2D* CreateTexture();

	void InvalidateGeneratedData();

	bool DoComponentsNeedUpdate(UCustomizableObjectInstance* CustomizableObjectInstance, const TSharedRef<FUpdateContextPrivate>& OperationData, bool& bOutEmptyMesh);

	mu::FResourceID GetLastMeshId(int32 ComponentIndex, int32 LODIndex) const;
	void SetLastMeshId(int32 ComponentIndex, int32 LODIndex, mu::FResourceID MeshId);

public:
	bool LoadParametersFromProfile(int32 ProfileIndex);
	
	bool SaveParametersToProfile(int32 ProfileIndex);
	
	bool MigrateProfileParametersToCurrentInstance(int32 ProfileIndex);

	void SetSelectedParameterProfileDirty();

	bool IsSelectedParameterProfileDirty() const;

	int32 GetState() const;

	void SetState(int32 InState);

	void AdditionalAssetsAsyncLoaded(UE::Tasks::FTaskEvent CompletionEvent);

	FCustomizableObjectInstanceDescriptor& GetDescriptor() const;
	
	UCustomizableObjectInstance* GetPublic() const;

	// If any components are using this instance, they will store the min of their distances to the player here every frame for LOD purposes
	float MinSquareDistFromComponentToPlayer = FLT_MAX;
	float LastMinSquareDistFromComponentToPlayer = FLT_MAX; // The same as the previous dist for last frame
												
	// This is the LODs that the Customizable Object has
	int32 NumLODsAvailable = INT32_MAX;

	// First SkeletalMesh LOD we can generate on the running platform
	uint8 FirstLODAvailable = 0;

	// First Non-streamable SkeletalMesh LOD
	uint8 FirstResidentLOD = MAX_MESH_LOD_COUNT;
	
	UPROPERTY(Transient)
	TArray<FCustomizableInstanceComponentData> ComponentsData;

	UPROPERTY(Transient)
	TArray< TObjectPtr<UMaterialInterface> > ReferencedMaterials;

	// Converts a ReferencedMaterials index from the CustomizableObject to an index in the ReferencedMaterials in the Instance
	TMap<uint32, uint32> ObjectToInstanceIndexMap;

	TArray<FGeneratedTexture> TexturesToRelease;

	UPROPERTY(Transient)
	TArray< TObjectPtr<UPhysicsAsset> > ClothingPhysicsAssets;

	// To keep loaded AnimBPs referenced and prevent GC
	UPROPERTY(Transient, Category = Animation, editfixedsize, VisibleAnywhere)
	TArray<TSubclassOf<UAnimInstance>> GatheredAnimBPs;

	UPROPERTY(Transient, Category = Animation, editfixedsize, VisibleAnywhere)
	FGameplayTagContainer AnimBPGameplayTags;

	UPROPERTY(Transient, Category = Animation, editfixedsize, VisibleAnywhere)
	TMap<TSubclassOf<UAnimInstance>, FAnimBpGeneratedPhysicsAssets> AnimBpPhysicsAssets;

	// The pass-through textures that will be loaded during an update
	TArray<TSoftObjectPtr<UTexture>> PassThroughTexturesToLoad;

	// Used during an update to prevent the pass-through textures loaded by LoadAdditionalAssetsAsync() from being unloaded by GC
	// between AdditionalAssetsAsyncLoaded() and their setting into the generated materials in BuildMaterials()
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture>> LoadedPassThroughTexturesPendingSetMaterial;

private:
	ECOInstanceFlags InstanceFlagsPrivate = ECOInstanceFlags::ECONone;

public:
	/** Copy of the descriptor of the latest successful update. */
	UPROPERTY(Transient)
	FCustomizableObjectInstanceDescriptor CommittedDescriptor;

	/** Hash of the descriptor copy of the latest successful update. */
	FDescriptorHash CommittedDescriptorHash;
	
	/** Status of the generated Skeletal Mesh. Not to be confused with the Update Result. */
	ESkeletalMeshStatus SkeletalMeshStatus = ESkeletalMeshStatus::NotGenerated;

	TMap<FString, bool> ParamNameToExpandedMap; // Used to check whether a mutable param is expanded in the editor to show its child params

#if WITH_EDITOR
	/** During editor, always remember the duration of the last update in the mutable runtime, for profiling. */
	int32 LastUpdateMutableRuntimeCycles = 0;
#endif

	bool bShowOnlyRuntimeParameters = true;
	bool bShowOnlyRelevantParameters = true;
	bool bShowUISections = false;
	
	// TEMP VARIABLE to check the Min desired LODs for this instance
	TWeakObjectPtr<UCustomizableObjectInstanceUsage> NearestToActor;
	TWeakObjectPtr<const AActor> NearestToViewCenter;

#if WITH_EDITOR
	/** Profile index the instance parameters are in and if the profile needs to be refreshed */
	int32 SelectedProfileIndex = INDEX_NONE;
	bool bSelectedProfileDirty = false;
#endif 
	
#if WITH_EDITORONLY_DATA
	/** Preview Instance Properties search box filter. Saved here to avoid losing the text during UI refreshes. */
	FText ParametersSearchFilter;
#endif
};

