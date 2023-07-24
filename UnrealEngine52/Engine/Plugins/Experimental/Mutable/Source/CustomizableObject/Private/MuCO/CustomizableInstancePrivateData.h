// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Materials/MaterialInterface.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"

#include "GameplayTagContainer.h"

#include "UObject/Package.h"
#include "CustomizableInstancePrivateData.generated.h"

namespace mu { class PhysicsBody; }
struct FMutableModelImageProperties;
struct FMutableRefSkeletalMeshData;
struct FStreamableHandle;

/** CustomizableObject Instance flags for internal use  */
enum ECOInstanceFlags
{
	ECONone							= 0,  // Should not use the name None here.. it collides with other enum in global namespace

	// Update process
	Updating						= 1 << 0,	//
	CreatingSkeletalMesh			= 1 << 1,	//
	Generated						= 1 << 2,	//
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

	// Streaming
	LODsStreamingEnabled			= 1 << 11,	// Stream LODs instead of generating all LODs at once. Enables LODs update(upgrade)/downgrade.
	
	// Generation
	ForceGenerateAllLODs			= 1 << 12,	// If set, Requested LOD Levels will be ignored and all LODs in between the current min/max lod will be generated
};


USTRUCT()
struct FGeneratedTexture
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(Category = CustomizableObjectInstance, EditAnywhere)
	int32 Id = 0;

	UPROPERTY(Category = CustomizableObjectInstance, EditAnywhere) 
	FString Name;

	UPROPERTY(Category = CustomizableObjectInstance, EditAnywhere) 
	TObjectPtr<UTexture2D> Texture = nullptr;
};


USTRUCT()
struct FGeneratedMaterial
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(Category = CustomizedMaterial, VisibleAnywhere)
	TArray< FGeneratedTexture > Textures;
};


// Unreal-side data for the parameter decorations.
USTRUCT()
struct FParameterDecorations
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(Transient)
	TArray< TObjectPtr<UTexture2D> > Images;
};


USTRUCT()
struct FReferencedPhysicsAssets
{
	GENERATED_USTRUCT_BODY();
	
	TArray<FString> PhysicsAssetToLoad;
	
	UPROPERTY(Transient)
	TArray< TObjectPtr<UPhysicsAsset> > PhysicsAssetsToMerge;
};


USTRUCT()
struct FReferencedSkeletons
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<int32> SkeletonsToLoad;

	UPROPERTY()
	TArray< TObjectPtr<USkeleton> > SkeletonsToMerge;
};


USTRUCT()
struct FCustomizableInstanceComponentData
{
	GENERATED_USTRUCT_BODY();

	uint16 ComponentIndex = 0;

	// AnimBP data gathered for a component from its constituent meshes
	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	TMap<int32, TSoftClassPtr<UAnimInstance>> AnimSlotToBP;

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
	TArray<TPair<int32, FString>> ClothingPhysicsAssetsToStream;

	/** Array of generated MeshIds per each LOD, used to decide if the mesh should be updated or not.
	 *  Size == NumLODsAvailable
	 *  LODs without mesh will be set to -1 */
	TArray<int32> LastMeshIdPerLOD;

	bool operator==(const FCustomizableInstanceComponentData& Other) const { return ComponentIndex == Other.ComponentIndex; }
};


UCLASS()
class UCustomizableInstancePrivateData : public UObject
{
public:
	GENERATED_BODY()

	UCustomizableInstancePrivateData();

	UPROPERTY( Transient )
	TArray<FGeneratedMaterial> GeneratedMaterials;

	UPROPERTY( Transient )
	TArray<FGeneratedTexture> GeneratedTextures;

	// Projector state for each parameter name
	TMap<TPair<FString, int32>, EProjectorState::Type> ProjectorStates;

	// Parameter decoration unreal-side data if generated.
	UPROPERTY(Transient)
	TArray<FParameterDecorations> ParameterDecorations;

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

	FCustomizableInstanceComponentData* GetComponentData(int32 ComponentIndex);
	const FCustomizableInstanceComponentData* GetComponentData(int32 ComponentIndex) const;

	ECOInstanceFlags GetCOInstanceFlags() const { return InstanceFlagsPrivate; }
	void SetCOInstanceFlags(ECOInstanceFlags FlagsToSet) { InstanceFlagsPrivate = (ECOInstanceFlags)(InstanceFlagsPrivate | FlagsToSet); }
	void ClearCOInstanceFlags(ECOInstanceFlags FlagsToClear) { InstanceFlagsPrivate = (ECOInstanceFlags)(InstanceFlagsPrivate & ~FlagsToClear); }
	bool HasCOInstanceFlags(ECOInstanceFlags FlagsToCheck) const { return (InstanceFlagsPrivate & FlagsToCheck) != 0; }

	void BuildMaterials(const TSharedPtr<FMutableOperationData>& OperationData, UCustomizableObjectInstance* Public);

	void ReuseTexture(UTexture2D* Texture);

	// Return an event that will be fired when the assets  have been loaded. It returns null if no asset needs loading.
	FGraphEventRef LoadAdditionalAssetsAsync(const TSharedPtr<FMutableOperationData>& OperationData, UCustomizableObjectInstance* Public, struct FStreamableManager &StreamableManager);
	void AdditionalAssetsAsyncLoaded(UCustomizableObjectInstance* Public);

	/** Temporal function. Used by GetParameters and ReloadParameters but its body should be moved somewhere else (probably on some other Instance update function). */
	void InstanceUpdateFlags(const UCustomizableObjectInstance& Public);

	/** See FCustomizableObjectInstanceDescriptor::GetParameters(...). */
	mu::ParametersPtr GetParameters(UCustomizableObjectInstance* Public);

	/** See FCustomizableObjectInstanceDescriptor::ReloadParameters(...). */
	void ReloadParameters(UCustomizableObjectInstance* Public);

	void TickUpdateCloseCustomizableObjects(UCustomizableObjectInstance& Public);
	void UpdateInstanceIfNotGenerated(UCustomizableObjectInstance& Public);

	// Returns true if success (?)
	bool UpdateSkeletalMesh_PostBeginUpdate0(UCustomizableObjectInstance* Public, const TSharedPtr<FMutableOperationData>& OperationData);

	static void ReleaseMutableTexture(int32 MutableTextureId, UTexture2D* Texture, struct FMutableResourceCache& Cache);

	// Parameter decoration generation: game thread step
	void UpdateParameterDecorationsEngineResources(const TSharedPtr<FMutableOperationData>& OperationData);

	// Prepare the data for the unreal textures, but don't create them because
	// it runs in the mutable thread
	// \TODO: CustomizableObject shouldn't be here
	static void ProcessTextureCoverageQueries(const TSharedPtr<FMutableOperationData>& OperationData, UCustomizableObject* CustomizableObject, const FString& ImageKeyName, FTexturePlatformData *PlatformData, UMaterialInterface* Material);

	// Prepare the data for the unreal reference skeleton
	// it runs in the mutable thread
	static void PrepareSkeletonData(const TSharedPtr<FMutableOperationData>& OperationData);

	// Copy data generated in the mutable thread over to the instance and initializes additional data required during the update
	void PrepareForUpdate(const TSharedPtr<FMutableOperationData>& OperationData);

	int32 GetNumLODsAvailable() const { return NumLODsAvailable; }
	
	// The following method is basically copied from PostEditChangeProperty and/or SkeletalMesh.cpp to be able to replicate PostEditChangeProperty without the editor
	void PostEditChangePropertyWithoutEditor(USkeletalMesh* SkeletalMesh);
	
	void DoUpdateSkeletalMesh(UCustomizableObjectInstance& Instance, bool bIsCloseDistTick=false, bool bOnlyUpdateIfNotGenerated=false, bool bIgnoreCloseDist=false, bool bForceHighPriority = false);

	void DiscardResourcesAndSetReferenceSkeletalMesh(UCustomizableObjectInstance* Public);

	/** 
	* \param OnlyLOD: If not 0, extract and convert only one single LOD from the source image.
	* \param ExtractChannel: If different than -1, extract a single-channel image with the specified source channel data.
	*/
	CUSTOMIZABLEOBJECT_API static void ConvertImage(class UTexture2D* Texture, mu::ImagePtrConst MutableImage, const FMutableModelImageProperties& Props, int32 OnlyLOD=-1, int32 ExtractChannel=-1);

	/** Set OnlyLOD to -1 to generate all mips */
	CUSTOMIZABLEOBJECT_API static FTexturePlatformData* MutableCreateImagePlatformData(mu::Ptr<const mu::Image> MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY);

private:

	void InitLastUpdateData(const TSharedPtr<FMutableOperationData>& OperationData);

	void InitSkeletalMeshData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, int32 ComponentIndex);

	bool BuildSkeletonData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, UCustomizableObjectInstance* CustomizableObjectIntance, int32 ComponentIndex);
	void BuildMeshSockets(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, UCustomizableObjectInstance* CustomizableObjectInstance, mu::MeshPtrConst MutableMesh);
	void BuildOrCopyElementData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	void BuildOrCopyMorphTargetsData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	bool BuildOrCopyRenderData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	void BuildOrCopyClothingData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* SrcSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);

	bool CopySkeletonData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SrcSkeletalMesh, USkeletalMesh* DestSkeletalMesh, int32 ComponentIndex);
	void CopyMeshSockets(USkeletalMesh* SrcSkeletalMesh, USkeletalMesh* DestSkeletalMesh);

	//
	USkeleton* MergeSkeletons(UCustomizableObjectInstance* Public, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, int32 ComponentIndex);

	//
	UPhysicsAsset* GetOrBuildPhysicsAsset(TObjectPtr<class UPhysicsAsset> TamplateAsset, const mu::PhysicsBody* PhysicsBody, int32 ComponentIndex, bool bDisableCollisionBetweenAssets);
	
	// Create a transient texture and add it to the TextureTrackerArray
	UTexture2D* CreateTexture();

	
	void InvalidateGeneratedData();

	bool DoComponentsNeedUpdate(UCustomizableObjectInstance* CustomizableObjectInstance, const TSharedPtr<FMutableOperationData>& OperationData, TArray<bool>& OutComponentNeedsUpdate, bool& bOutEmptyMesh);

	int32 GetLastMeshId(int32 ComponentIndex, int32 LODIndex) const;
	void SetLastMeshId(int32 ComponentIndex, int32 LODIndex, int32 MeshId);

public:

	// If any components are using this instance, they will store the min of their distances to the player here every frame for LOD purposes
	float MinSquareDistFromComponentToPlayer;
	float LastMinSquareDistFromComponentToPlayer; // The same as the previous dist for last frame
												
	// This is the LODs that the Customizable Object has
	int32 NumLODsAvailable;

	// First SkeletalMesh LOD we can generate on the running platform
	uint8 FirstLODAvailable;

	// Maximum number of SkeletalMesh LODs to stream
	uint8 NumMaxLODsToStream;
	
	TMap<FString, FTextureCoverageQueryData> TextureCoverageQueries;

	UPROPERTY(Transient)
	TArray<FCustomizableInstanceComponentData> ComponentsData;

	UPROPERTY(Transient)
	TArray< TObjectPtr<UMaterialInterface> > ReferencedMaterials;

	// Converts a ReferencedMaterials index from the CustomizableObject to an index in the ReferencedMaterials in the Instance
	TMap<uint32, uint32> ObjectToInstanceIndexMap;

	TMap<uint32, FGeneratedTexture> TexturesToRelease;

	UPROPERTY(Transient)
	TArray< TObjectPtr<UPhysicsAsset> > ClothingPhysicsAssets;

	// To keep loaded AnimBPs referenced and prevent GC
	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	TArray<TSubclassOf<UAnimInstance>> GatheredAnimBPs;

	UPROPERTY(Transient, Category = CustomizableObjectInstance, editfixedsize, VisibleAnywhere)
	FGameplayTagContainer AnimBPGameplayTags;

	// Struct used during an update to avoid generating resources that can be reused.
	FInstanceGeneratedData LastUpdateData;

private:
	
	ECOInstanceFlags InstanceFlagsPrivate = ECOInstanceFlags::ECONone;

	// Struct used by BuildMaterials() to identify common materials between LODs
	struct FMutableMaterialPlaceholder
	{
		enum class EPlaceHolderParamType { Vector, Scalar, Texture };

		struct FMutableMaterialPlaceHolderParam
		{
			FName ParamName;
			int32 LayerIndex; // Set to -1 for non-multilayer params
			FLinearColor Vector;
			float Scalar;
			FGeneratedTexture Texture;
			EPlaceHolderParamType Type;

			FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const FLinearColor& InVector)
				: ParamName(InParamName), LayerIndex(InLayerIndex), Vector(InVector), Type(EPlaceHolderParamType::Vector) {}

			FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const float InScalar)
				: ParamName(InParamName), LayerIndex(InLayerIndex), Scalar(InScalar), Type(EPlaceHolderParamType::Scalar) {}

			FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const FGeneratedTexture& InTexture)
				: ParamName(InParamName), LayerIndex(InLayerIndex), Texture(InTexture), Type(EPlaceHolderParamType::Texture) {}
		};

		UMaterialInterface* ParentMaterial;
		TArray<FMutableMaterialPlaceHolderParam> Params;
		int32 MatIndex = -1;

		void AddParam(const FMutableMaterialPlaceHolderParam& NewParam) { Params.Add(NewParam); }

		FString GetSerialization() const
		{
			FString Serialization = ParentMaterial ? FString::FromInt(ParentMaterial->GetUniqueID()) : FString("null");

			for (const FMutableMaterialPlaceHolderParam& Param : Params)
			{
				Serialization += FString("-");
				Serialization += FString::FromInt((int32)Param.Type) + FString("_") + Param.ParamName.ToString() + FString("_")
					+ FString::FromInt(Param.LayerIndex) + FString("_");

				switch (Param.Type)
				{
				case EPlaceHolderParamType::Vector:
					Serialization += Param.Vector.ToString();
					break;

				case EPlaceHolderParamType::Scalar:
					Serialization += FString::Printf(TEXT("%f"), Param.Scalar);
					break;

				case EPlaceHolderParamType::Texture:
					Serialization += FString::FromInt(Param.Texture.Texture->GetUniqueID());
					break;
				}
			}

			return Serialization;
		}
	};
};

