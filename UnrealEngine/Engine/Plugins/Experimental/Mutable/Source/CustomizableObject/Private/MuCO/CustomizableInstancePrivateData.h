// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Model.h"
#include "MuR/System.h"
#include "MuR/Mesh.h"
#include "MuR/Image.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"

#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "Engine/Texture2D.h"

#include "ClothingAsset.h"
#include "GameplayTagContainer.h"

#include "CustomizableInstancePrivateData.generated.h"

/** CustomizableObject Instance flags for internal use  */
enum ECOInstanceFlags
{
	None							= 0,

	// Update process
	Updating						= 1 << 0,	//
	CreatingSkeletalMesh			= 1 << 1,	//
	Generated						= 1 << 2,	//
	AssetsLoaded					= 1 << 3,	// Assets required to generate a mesh. Mainly Materials, PhysicsAssets and AnimBps 
	ReduceLODs						= 1 << 4,	// CurrentMinLOD will be mapped to the LOD 0 of the generated SkeletalMesh. 
	ReuseTextures					= 1 << 5, 	// 
	ReplacePhysicsAssets			= 1 << 6,	// Merge active PhysicsAssets and replace the base physics asset
	PendingMeshUpdate				= 1 << 7,	// Internaly used to know if we should regenerate the SkeletalMeshes when updating the instance

	// Update priorities
	UsedByComponent					= 1 << 8,	// If any components are using this instance, they will set flag every frame
	UsedByComponentInPlay			= 1 << 9,	// If any components are using this instance in play, they will set flag every frame
	UsedByPlayerOrNearIt			= 1 << 10,	// The instance is used by the player or is near the player, used to give more priority to its updates
	DiscardedByNumInstancesLimit	= 1 << 11,	// The instance is descarded because we exceeded the limit of instances generated 

	// Types of updates
	PendingLODsUpdate				= 1 << 12,	// Used to queue an update due to a change in LODs required by the instance
	PendingLODsUpdateSecondStage	= 1 << 13,	// Second stage of the previous flag, Internaly used to queue an update due to a change in LODs required by the instance
	PendingLODsDowngrade			= 1 << 14,	// Used to queue a downgrade update to reduce the number of LODs. LOD update goes from a high res level to a low res one, ex: 0 to 1 or 1 to 2

	// Streaming
	LODsStreamingEnabled			= 1 << 15,	// Stream LODs instead of generating all LODs at once. Enables LODs update(upgrade)/downgrade.
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
	
	mu::ParametersPtr ReloadParametersFromObject(UCustomizableObjectInstance* Public, bool ClearLastMeshIds = false);

	void TickUpdateCloseCustomizableObjects(UCustomizableObjectInstance* Public);
	void UpdateInstanceIfNotGenerated(UCustomizableObjectInstance* Public, bool bAsync);

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

	void SetMinMaxLODToLoad(UCustomizableObjectInstance* Public, int32 NewMinLOD, int32 NewMaxLOD, bool bLimitLODUpgrades = true);
	
	int32 GetNumLODsAvailable() const { return NumLODsAvailable; }
	
	// The following method is basically copied from PostEditChangeProperty and/or SkeletalMesh.cpp to be able to replicate PostEditChangeProperty without the editor
	void PostEditChangePropertyWithoutEditor(USkeletalMesh* SkeletalMesh);
	
	void DoUpdateSkeletalMeshAsync(UCustomizableObjectInstance* Instance, FMutableQueueElem::EQueuePriorityType Priority = FMutableQueueElem::EQueuePriorityType::Low);
	void DoUpdateSkeletalMesh(UCustomizableObjectInstance* Instance, bool bAsync, bool bIsCloseDistTick=false, bool bOnlyUpdateIfNotGenerated=false, bool bIgnoreCloseDist=false, bool bForceHighPriority = false);

	void DiscardResourcesAndSetReferenceSkeletalMesh(UCustomizableObjectInstance* Public);

	/** 
	* \param OnlyLOD: If not 0, extract and convert only one single LOD from the source image.
	* \param ExtractChannel: If different than -1, extract a single-channel image with the specified source channel data.
	*/
	CUSTOMIZABLEOBJECT_API static void ConvertImage(class UTexture2D* Texture, mu::ImagePtrConst MutableImage, const FMutableModelImageProperties& Props, int32 OnlyLOD=-1, int32 ExtractChannel=-1);

	/** Set OnlyLOD to -1 to generate all mips */
	CUSTOMIZABLEOBJECT_API static FTexturePlatformData* MutableCreateImagePlatformData(const mu::Image* MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY);

private:

	bool BuildSkeletalMeshSkeletonData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, UCustomizableObjectInstance* CustomizableObjectIntance, int32 ComponentIndex);
	void BuildMorphTargetsData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	void BuildClothingData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	void BuildSkeletalMeshElementData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);
	bool BuildSkeletalMeshRenderData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex);

	//
	USkeleton* MergeSkeletons(UCustomizableObjectInstance* Public, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, int32 ComponentIndex);

	//
	UPhysicsAsset* BuildPhysicsAsset(TObjectPtr<class UPhysicsAsset> TamplateAsset, const mu::PhysicsBody* PhysicsBody, int32 ComponentIndex, bool bDisableCollisionBetweenAssets);
	
	
	int32 GetLastMeshId(int32 ComponentIndex, int32 LODIndex) const;
	void SetLastMeshId(int32 ComponentIndex, int32 LODIndex, int32 MeshId);
	void ClearAllLastMeshIds();

	bool MeshNeedsUpdate(UCustomizableObjectInstance* Public, const TSharedPtr<FMutableOperationData>& OperationData, bool& bOutEmptyMesh);

	UTexture2D* CreateTexture();
	
public:

	// If any components are using this instance, they will store the min of their distances to the player here every frame for LOD purposes
	float MinSquareDistFromComponentToPlayer;
	float LastMinSquareDistFromComponentToPlayer; // The same as the previous dist for last frame
												
	double LastUpdateTime;

	// LODs applied on the last update. Represent the actual LODs the Instance is using.
	int32 LastUpdateMinLOD = -1;
	int32 LastUpdateMaxLOD = -1;

	/** Saves the LODs requested on the update. */
	void SaveMinMaxLODToLoad(const UCustomizableObjectInstance* Public);
	
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

private:
	
	ECOInstanceFlags InstanceFlagsPrivate = ECOInstanceFlags::None;

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

