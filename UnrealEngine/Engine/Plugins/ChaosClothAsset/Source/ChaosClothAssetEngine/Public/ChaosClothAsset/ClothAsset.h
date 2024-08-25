// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/SkinnedAsset.h"
#include "ReferenceSkeleton.h"
#include "RenderCommandFence.h"
#include "ClothAsset.generated.h"

class FSkeletalMeshRenderData;
class FSkeletalMeshModel;
class FSkinnedAssetCompilationContext;
class USkeletalMesh;
class UDataflow;
struct FChaosClothSimulationModel;
struct FSkeletalMeshLODInfo;
struct FManagedArrayCollection;
struct FChaosClothAssetLodTransitionDataCache;
class UAnimationAsset;

UENUM()
enum class EClothAssetAsyncProperties : uint64
{
	None = 0,
	RenderData = 1 << 0,
	ThumbnailInfo = 1 << 1,
	ImportedModel = 1 << 2,
	ClothCollection = 1 << 3,
	RefSkeleton = 1 << 4,
	All = MAX_uint64
};
ENUM_CLASS_FLAGS(EClothAssetAsyncProperties);

/**
 * Cloth asset for pattern based simulation.
 */
UCLASS(hidecategories = Object, BlueprintType)
class CHAOSCLOTHASSETENGINE_API UChaosClothAsset : public USkinnedAsset
{
	GENERATED_BODY()
public:
	UChaosClothAsset(const FObjectInitializer& ObjectInitializer);
	UChaosClothAsset(FVTableHelper& Helper);  // This is declared so we can use TUniquePtr<FClothSimulationModel> with just a forward declare of that class
	virtual ~UChaosClothAsset() override;

	//~ Begin UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject interface

	//~ Begin USkinnedAsset interface
	virtual FReferenceSkeleton& GetRefSkeleton()								
	{
		WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::RefSkeleton);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RefSkeleton;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	virtual const FReferenceSkeleton& GetRefSkeleton() const					
	{
		WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::RefSkeleton, ESkinnedAssetAsyncPropertyLockType::ReadOnly);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return RefSkeleton;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	virtual FSkeletalMeshLODInfo* GetLODInfo(int32 Index);
	virtual const FSkeletalMeshLODInfo* GetLODInfo(int32 Index) const;
	UFUNCTION(BlueprintGetter)
	virtual class UPhysicsAsset* GetShadowPhysicsAsset() const override			{ return ShadowPhysicsAsset; }
	virtual FMatrix GetComposedRefPoseMatrix(FName InBoneName) const override;
	virtual FMatrix GetComposedRefPoseMatrix(int32 InBoneIndex) const override	{ return CachedComposedRefPoseMatrices[InBoneIndex]; }
	virtual const FMeshUVChannelInfo* GetUVChannelData(int32 MaterialIndex) const override;
	virtual bool GetSupportRayTracing() const override							{ return bSupportRayTracing; }
	virtual int32 GetRayTracingMinLOD() const override							{ return RayTracingMinLOD; }
	virtual TArray<FMatrix44f>& GetRefBasesInvMatrix() override					{ return RefBasesInvMatrix; }
	virtual const TArray<FMatrix44f>& GetRefBasesInvMatrix() const override		{ return RefBasesInvMatrix; }
	virtual TArray<FSkeletalMeshLODInfo>& GetLODInfoArray() override			{ return LODInfo; }
	virtual const TArray<FSkeletalMeshLODInfo>& GetLODInfoArray() const override { return LODInfo; }
	virtual FSkeletalMeshRenderData* GetResourceForRendering() const override;
	virtual int32 GetDefaultMinLod() const										{ return 0; }
	virtual UPhysicsAsset* GetPhysicsAsset() const								{ return PhysicsAsset; }
	virtual TArray<FSkeletalMaterial>& GetMaterials() override					{ return Materials; }
	virtual const TArray<FSkeletalMaterial>& GetMaterials() const override		{ return Materials; }
	virtual int32 GetLODNum() const override									{ return LODInfo.Num(); }
	virtual bool IsMaterialUsed(int32 MaterialIndex) const override				{ return true; }
	virtual FBoxSphereBounds GetBounds() const override							{ return Bounds; }
	virtual TArray<class USkeletalMeshSocket*> GetActiveSocketList() const override { static TArray<class USkeletalMeshSocket*> Dummy; return Dummy; }
	virtual USkeletalMeshSocket* FindSocket(FName InSocketName) const override	{ return nullptr; }
	virtual USkeletalMeshSocket* FindSocketInfo(FName InSocketName, FTransform& OutTransform, int32& OutBoneIndex, int32& OutIndex) const override { return nullptr; }
	virtual USkeleton* GetSkeleton() override									{ return Skeleton; }  // Note: The USkeleton isn't a reliable source of reference skeleton
	virtual const USkeleton* GetSkeleton() const override						{ return Skeleton; }
	virtual void SetSkeleton(USkeleton* InSkeleton) override					{ Skeleton = InSkeleton; }
	virtual UMeshDeformer* GetDefaultMeshDeformer() const override				{ return nullptr; }
	virtual class UMaterialInterface* GetOverlayMaterial() const override		{ return nullptr; }
	virtual float GetOverlayMaterialMaxDrawDistance() const override			{ return 0.f; }
	virtual bool IsValidLODIndex(int32 Index) const override					{ return LODInfo.IsValidIndex(Index); }
	virtual int32 GetMinLodIdx(bool bForceLowestLODIdx = false) const override;
	virtual bool NeedCPUData(int32 LODIndex) const override						{ return false; }
	virtual bool GetHasVertexColors() const override							{ return false; }
	virtual int32 GetPlatformMinLODIdx(const ITargetPlatform* TargetPlatform) const override;
	virtual const FPerPlatformBool& GetDisableBelowMinLodStripping() const override { return DisableBelowMinLodStripping; }
	virtual const FPerPlatformInt& GetMinLod() const override;
#if WITH_EDITOR
	/* Build a LOD model for the targeted platform. */
	virtual void BuildLODModel(const ITargetPlatform* TargetPlatform, int32 LODIndex) override;
	virtual FString BuildDerivedDataKey(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsInitialBuildDone() const override;
	virtual bool GetEnableLODStreaming(const class ITargetPlatform* TargetPlatform) const override	{ return false; }
	virtual int32 GetMaxNumStreamedLODs(const class ITargetPlatform* TargetPlatform) const override	{ return 0; }
	virtual int32 GetMaxNumOptionalLODs(const ITargetPlatform* TargetPlatform) const override { return 0; }
#endif
#if WITH_EDITORONLY_DATA
	virtual FSkeletalMeshModel* GetImportedModel() const override
	{
		WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::ImportedModel);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return MeshModel.Get();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
#endif
	//~ End USkinnedAsset interface

	/** Return the enclosed Cloth Collection object. */
	TArray<TSharedRef<FManagedArrayCollection>>& GetClothCollections() 
	{
		WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::ClothCollection);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return ClothCollections;
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Return the enclosed Cloth Collection object, const version. */
	const TArray<TSharedRef<const FManagedArrayCollection>>& GetClothCollections() const 
	{
		WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties::ClothCollection, ESkinnedAssetAsyncPropertyLockType::ReadOnly);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return reinterpret_cast<const TArray<TSharedRef<const FManagedArrayCollection>>&>(ClothCollections);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Return the cloth simulation ready LOD model data. */
	TSharedPtr<const FChaosClothSimulationModel> GetClothSimulationModel() const { return ClothSimulationModel; }

	/** Build this asset static render and simulation data. This needs to be done every time the asset has changed. */
	void Build(TArray<FChaosClothAssetLodTransitionDataCache>* InOutTransitionCache = nullptr);

	/**
	 * Copy the draped simulation mesh patterns into the render mesh data.
	 * This is useful to visualize the simulation mesh, or when the simulation mesh can be used for both simulation and rendering.
	 * @param Material The material used to render. If none is specified the default debug cloth material will be used.
	 */
	void CopySimMeshToRenderMesh(UMaterialInterface* Material = nullptr);

	/** Set the physics asset for this cloth. */
	void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset);

	/**
	 * Set the skeleton asset using the LOD0 reference skeleton of the skeletal mesh specified in the cloth collection, or set a default reference skeleton if there isn't any.
	 * @param bRebuildModels Whether to rebuild the simulation and rendering models (editor builds only).
	 */
	void UpdateSkeletonFromCollection(bool bRebuildModels);

	/**
	 * Set the specified reference skeleton for this cloth, and rebuild the models.
	 * Use nullptr to set to a default single bone root reference skeleton.
	 * @param bRebuildModels Whether to rebuild the mesh static models. This is always required, but could be skipped if done soon after.
	 * @param bRebindMeshes Bindings could be invalid after a change of reference skeleton, use this option to clear the existing binding and bind the sim and render meshes to the root bone.
	 */
	void SetReferenceSkeleton(const FReferenceSkeleton* ReferenceSkeleton, bool bRebuildModels = true, bool bRebindMeshes = true);

	/** Set the bone hierachy to use for this cloth. */
	UE_DEPRECATED(5.3, "Use SetReferenceSkeleton(const FReferenceSkeleton*, bool, bool) instead")
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	void SetReferenceSkeleton(const FReferenceSkeleton& InReferenceSkeleton, bool bRebuildClothSimulationModel = true) { GetRefSkeleton() = InReferenceSkeleton; UpdateSkeleton(bRebuildClothSimulationModel); }
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Set the skinning weights for all of the sim vertices to be bound to the root node of the reference skeleton. */
	UE_DEPRECATED(5.3, "Use FClothGeometryTools::BindMeshToRootBone or SetReferenceSkeleton(const FReferenceSkeleton*, bool, bool) instead.")
	void BindSimMeshToRootBone();

	//
	// Dataflow
	//
	UE_DEPRECATED(5.3, "Do not use. Will be made private in 5.4")
	UPROPERTY(EditAnywhere, Category = "Dataflow")
	TObjectPtr<UDataflow> DataflowAsset;

	UE_DEPRECATED(5.3, "Do not use. Will be made private in 5.4")
	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FString DataflowTerminal = "ClothAssetTerminal";


#if WITH_EDITORONLY_DATA

	void SetPreviewSceneSkeletalMesh(USkeletalMesh* Mesh);
	USkeletalMesh* GetPreviewSceneSkeletalMesh() const;

	void SetPreviewSceneAnimation(UAnimationAsset* Animation);
	UAnimationAsset* GetPreviewSceneAnimation() const;

#endif

private:
	//~ Begin USkinnedAsset interface
	/** Initial step for the Post Load process - Can't be done in parallel. */
	virtual void BeginPostLoadInternal(FSkinnedAssetPostLoadContext& Context) override;
	/** Thread-safe part of the Post Load */
	virtual void ExecutePostLoadInternal(FSkinnedAssetPostLoadContext& Context) override;
	/** Complete the postload process - Can't be done in parallel. */
	virtual void FinishPostLoadInternal(FSkinnedAssetPostLoadContext& Context) override;
	/** Convert async property from enum value to string. */
	virtual FString GetAsyncPropertyName(uint64 Property) const override;
	//~ End USkinnedAsset interface

	/**
	 * Wait for the asset to finish compilation to protect internal skinned asset data from race conditions during async build.
	 * This should be called before accessing all async accessible properties.
	 */
	void WaitUntilAsyncPropertyReleased(EClothAssetAsyncProperties AsyncProperties, ESkinnedAssetAsyncPropertyLockType LockType = ESkinnedAssetAsyncPropertyLockType::ReadWrite) const;

	/** Pre-calculate refpose-to-local transforms. */
	void CalculateInvRefMatrices();

	/** Re-calculate the bounds for this asset. */
	void CalculateBounds();

	/** Update the bone informations after a change of skeleton. */
	UE_DEPRECATED(5.3, "Use Build() instead")
	void UpdateSkeleton(bool bRebuildClothSimulationModel = true);

#if WITH_EDITOR
	/** Build the SkeletalMeshLODModel for this asset. */
	void BuildMeshModel();
#endif

	/** Build the clothing simulation meshes from the Cloth Collection. */
	void BuildClothSimulationModel(TArray<FChaosClothAssetLodTransitionDataCache>* InOutTransitionCache = nullptr);

	/** Initialize all render resources. */
	void InitResources();

	/** Safely release the render data. */
	void ReleaseResources();

	/** Set render data. */
	void SetResourceForRendering(TUniquePtr<FSkeletalMeshRenderData>&& InSkeletalMeshRenderData);

#if WITH_EDITOR
	/** Load render data from DDC if the data is cached, otherwise generate render data and save into DDC */
	void CacheDerivedData(FSkinnedAssetCompilationContext* Context);

	/** Initial step for the building process - Can't be done in parallel. USkinnedAsset Interface. */
	virtual void BeginBuildInternal(FSkinnedAssetBuildContext& Context) override;

	/** Thread-safe part. USkinnedAsset Interface. */
	virtual void ExecuteBuildInternal(FSkinnedAssetBuildContext& Context) override;

	/** Complete the building process - Can't be done in parallel. USkinnedAsset Interface. */
	virtual void FinishBuildInternal(FSkinnedAssetBuildContext& Context) override;
#endif

	/** Reregister all components using this asset to reset the simulation in case anything has changed. */
	void ReregisterComponents();

	/** List of materials for this cloth asset. Set by the Dataflow evaluation. */
	UPROPERTY(EditAnywhere, Category = Materials, Meta = (EditCondition = "DataflowAsset == nullptr"))
	TArray<FSkeletalMaterial> Materials;

	/**
	 * Skeleton asset used at creation time.
	 * This is of limited use since this USkeleton's reference skeleton might not necessarily match the one created for this asset.
	 * Set by the Dataflow evaluation.
	 */
	UPROPERTY(EditAnywhere, Setter = SetSkeleton, Category = Skeleton, Meta = (EditCondition = "DataflowAsset == nullptr"))
	TObjectPtr<USkeleton> Skeleton;

	/** Physics asset used for collision. Set by the Dataflow evaluation. */
	UPROPERTY(EditAnywhere, Category = Collision, Meta = (EditCondition = "DataflowAsset == nullptr"))
	TObjectPtr<UPhysicsAsset> PhysicsAsset;

	/** Struct containing information for each LOD level, such as materials to use, and when use the LOD. Not currently editable or customizable through the Dataflow. */
	UPROPERTY(VisibleAnywhere, EditFixedSize, Category = LevelOfDetails)
	TArray<FSkeletalMeshLODInfo> LODInfo;

	UPROPERTY(EditAnywhere, Category = LODSettings)
	FPerPlatformBool DisableBelowMinLodStripping;

	UPROPERTY(EditAnywhere, Category = LODSettings, Meta = (DisplayName = "Minimum LOD"))
	FPerPlatformInt MinLod;

	/** Enable raytracing for this asset. */
	UPROPERTY(EditAnywhere, Category = RayTracing)
	uint8 bSupportRayTracing : 1;

	/** Minimum raytracing LOD for this asset. */
	UPROPERTY(EditAnywhere, Category = RayTracing)
	int32 RayTracingMinLOD;

	/** Whether to blend positions between the skinned/simulated transitions of the cloth render mesh. Only used when not overriden in Dataflow by the ProxyDefomer node. */
	UE_DEPRECATED(5.4, "Superseded by the ProxyDefomer node.")
	UPROPERTY()
	bool bSmoothTransition_DEPRECATED = true;

	/** Whether to use multiple triangle influences on the proxy wrap deformer to help smoothe deformations. Only used when not overriden in Dataflow by the ProxyDefomer node. */
	UE_DEPRECATED(5.4, "Superseded by the ProxyDefomer node.")
	UPROPERTY()
	bool bUseMultipleInfluences_DEPRECATED = false;

	/** The radius from which to get the multiple triangle influences from the simulated proxy mesh. Only used when not overriden in Dataflow with the ProxyDefomer node. */
	UE_DEPRECATED(5.4, "Superseded by the ProxyDefomer node.")
	UPROPERTY()
	float SkinningKernelRadius_DEPRECATED = 30.f;

	/**
	 * Physics asset whose shapes will be used for shadowing when components have bCastCharacterCapsuleDirectShadow or bCastCharacterCapsuleIndirectShadow enabled.
	 * Only spheres and sphyl shapes in the physics asset can be supported.  The more shapes used, the higher the cost of the capsule shadows will be.
	 */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, BlueprintGetter = GetShadowPhysicsAsset, Category = Lighting)
	TObjectPtr<class UPhysicsAsset> ShadowPhysicsAsset;

	/** A unique identifier as used by the section rendering code. */
	UPROPERTY()
	FGuid AssetGuid;

	/** Bounds for this asset. */
	FBoxSphereBounds Bounds;

	/** Mesh-space ref pose, where parent matrices are applied to ref pose matrices. */
	TArray<FMatrix> CachedComposedRefPoseMatrices;

	/** Cloth Collection containing this asset data. One per LOD. */
	UE_DEPRECATED(5.4, "This must be protected for async build, always use the accessors even internally.")
	TArray<TSharedRef<FManagedArrayCollection>> ClothCollections;

	/** Reference skeleton created from the provided skeleton asset. */
	UE_DEPRECATED(5.4, "This must be protected for async build, always use the accessors even internally.")
	FReferenceSkeleton RefSkeleton;

	/** Reference skeleton precomputed bases. */
	TArray<FMatrix44f> RefBasesInvMatrix;

	/** Rendering data. */
	TUniquePtr<FSkeletalMeshRenderData> SkeletalMeshRenderData;

	/** A fence which is used to keep track of the rendering thread releasing the static mesh resources. */
	FRenderCommandFence ReleaseResourcesFence;

#if WITH_EDITORONLY_DATA
	/** Source mesh geometry information (not used at runtime). */
	UE_DEPRECATED(5.4, "This must be protected for async build, always use the accessors even internally.")
	TSharedPtr<FSkeletalMeshModel> MeshModel;
#endif

	/** Simulation mesh Lods as fed to the solver for constraints creation. Ownership gets transferred to the proxy when it is changed during a simulation. */
	TSharedPtr<FChaosClothSimulationModel> ClothSimulationModel;


#if WITH_EDITORONLY_DATA

	/*
	* The following PreviewScene properties are modeled after PreviewSkeletalMesh in USkeleton
	*	- they are inside WITH_EDITORONLY_DATA because they are not used at game runtime
	*	- TSoftObjectPtrs since that will make it possible to avoid loading these assets until the PreviewScene asks for them
	*	- DuplicateTransient so that if you copy a ClothAsset it won't copy these preview properties
	*	- AssetRegistrySearchable makes it so that if the user searches the name of a PreviewScene asset in the Asset Browser, it will return any ClothAssets that use it
	*/

	/** Optional Skeletal Mesh that the cloth asset is attached to in the Preview Scene in the Cloth Editor */
	UPROPERTY(DuplicateTransient, AssetRegistrySearchable)
	TSoftObjectPtr<USkeletalMesh> PreviewSceneSkeletalMesh;

	/** Optional animation attached to PreviewSceneSkeletalMesh in the Preview Scene in the Cloth Editor */
	UPROPERTY(DuplicateTransient, AssetRegistrySearchable)
	TSoftObjectPtr<UAnimationAsset> PreviewSceneAnimation;

#endif

#if WITH_EDITOR
	struct FBuilder;
#endif
};
