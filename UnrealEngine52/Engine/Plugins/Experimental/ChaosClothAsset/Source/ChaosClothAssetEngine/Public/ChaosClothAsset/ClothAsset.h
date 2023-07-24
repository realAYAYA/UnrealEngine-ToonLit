// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/SkinnedAsset.h"
#include "ReferenceSkeleton.h"
#include "RenderCommandFence.h"
#include "ClothAsset.generated.h"

namespace UE::Chaos::ClothAsset { class FClothCollection; }
class FSkeletalMeshRenderData;
class FSkeletalMeshModel;
struct FChaosClothSimulationModel;
struct FSkeletalMeshLODInfo;
class UDataflow;

UENUM()
enum class EClothAssetAsyncProperties : uint64
{
	None = 0,
	RenderData = 1 << 0,
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
	virtual FReferenceSkeleton& GetRefSkeleton()								{ return RefSkeleton; }
	virtual const FReferenceSkeleton& GetRefSkeleton() const					{ return RefSkeleton; }
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
	virtual USkeleton* GetSkeleton() override									{ return nullptr; }
	virtual const USkeleton* GetSkeleton() const override						{ return nullptr; }
	virtual void SetSkeleton(USkeleton* InSkeleton) override					{}
	virtual UMeshDeformer* GetDefaultMeshDeformer() const override				{ return nullptr; }
	virtual bool IsValidLODIndex(int32 Index) const override					{ return LODInfo.IsValidIndex(Index); }
	virtual int32 GetMinLodIdx(bool bForceLowestLODIdx = false) const override	{ return 0; }
	virtual bool NeedCPUData(int32 LODIndex) const override						{ return false; }
	virtual bool GetHasVertexColors() const override							{ return false; }
	virtual int32 GetPlatformMinLODIdx(const ITargetPlatform* TargetPlatform) const override { return 0; }
	virtual const FPerPlatformBool& GetDisableBelowMinLodStripping() const override { return DisableBelowMinLodStripping; }
	virtual const FPerPlatformInt& GetMinLod() const override					{ return MinLod; }
#if WITH_EDITOR
	virtual FString BuildDerivedDataKey(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsInitialBuildDone() const override;
	virtual bool GetEnableLODStreaming(const class ITargetPlatform* TargetPlatform) const override	{ return false; }
	virtual int32 GetMaxNumStreamedLODs(const class ITargetPlatform* TargetPlatform) const override	{ return 0; }
	virtual int32 GetMaxNumOptionalLODs(const ITargetPlatform* TargetPlatform) const override { return 0; }
#endif
#if WITH_EDITORONLY_DATA
	virtual FSkeletalMeshModel* GetImportedModel() const override				{ return MeshModel.Get(); }
#endif
	//~ End USkinnedAsset interface

	/** Return the enclosed Cloth Collection object. */
	TSharedPtr<UE::Chaos::ClothAsset::FClothCollection> GetClothCollection() { return ClothCollection; }

	/** Return the enclosed Cloth Collection object, const version. */
	TSharedPtr<const UE::Chaos::ClothAsset::FClothCollection> GetClothCollection() const { return ClothCollection; }

	/** Return the cloth simulation ready LOD model data. */
	TSharedPtr<const FChaosClothSimulationModel> GetClothSimulationModel() const { return ClothSimulationModel; }

	/** Build this asset static render and simulation data. This needs to be done every time the asset has changed. */
	void Build();

	/**
	 * Copy the draped simulation mesh patterns into the render mesh data.
	 * This is useful to visualize the simulation mesh, or when the simulation mesh can be used for both simulation and rendering.
	 * @param MaterialIndex The index of the Materials array.
	 */
	void CopySimMeshToRenderMesh(int32 MaterialIndex);

	/** Set the physics asset for this cloth. */
	void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset) { PhysicsAsset = InPhysicsAsset; }

	/** Set the bone hierachy to use for this cloth. */
	void SetReferenceSkeleton(const FReferenceSkeleton& InReferenceSkeleton, bool bRebuildClothSimulationModel = true) { RefSkeleton = InReferenceSkeleton; UpdateSkeleton(bRebuildClothSimulationModel); }

	//
	// Dataflow
	//
	UPROPERTY(EditAnywhere, Category = "Dataflow")
	TObjectPtr<UDataflow> DataflowAsset;

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	FString DataflowTerminal = "ClothAssetTerminal";


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

	/** Re-calculate the reference bone index, from which the cloth gets transformed when the simulation is suspended. */
	void CalculateReferenceBoneIndex();

	/** Update the bone informations after a change of skeleton. */
	void UpdateSkeleton(bool bRebuildClothSimulationModel = true);

#if WITH_EDITORONLY_DATA
	/** Build the SkeletalMeshLODModel for this asset. */
	void BuildMeshModel();
#endif

	/** Build the clothing simulation meshes from the Cloth Collection. */
	void BuildClothSimulationModel();

	/** Initialize all render resources. */
	void InitResources();

	/** Safely release the render data. */
	void ReleaseResources();

	/** Set render data. */
	void SetResourceForRendering(TUniquePtr<FSkeletalMeshRenderData>&& InSkeletalMeshRenderData);

#if WITH_EDITOR
	/** Load render data from DDC if the data is cached, otherwise generate render data and save into DDC */
	void CacheDerivedData(FSkinnedAssetPostLoadContext* Context);
#endif

	/** Reregister all components using this asset to reset the simulation in case anything has changed. */
	void ReregisterComponents();

	// TODO: determine if it should be serialized or transient
	/** List of materials for this cloth asset. */
	UPROPERTY(EditAnywhere, Category = Materials)
	TArray<FSkeletalMaterial> Materials;

	/** Physics asset used for collision. */
	UPROPERTY(EditAnywhere, Category = Collision)
	TObjectPtr<UPhysicsAsset> PhysicsAsset;

	/** Struct containing information for each LOD level, such as materials to use, and when use the LOD. */
	UPROPERTY(EditAnywhere, EditFixedSize, Category = LevelOfDetails)
	TArray<FSkeletalMeshLODInfo> LODInfo;

	UPROPERTY(EditAnywhere, Category = LODSettings)
	FPerPlatformBool DisableBelowMinLodStripping;

	UPROPERTY(EditAnywhere, Category = LODSettings, meta = (DisplayName = "Minimum LOD"))
	FPerPlatformInt MinLod;

	/** Enable raytracing for this asset. */
	UPROPERTY(EditAnywhere, Category = RayTracing)
	uint8 bSupportRayTracing : 1;

	/** Minimum raytracing LOD for this asset. */
	UPROPERTY(EditAnywhere, Category = RayTracing)
	int32 RayTracingMinLOD;

	/** Whether to blend positions between the skinned/simulated transitions of the cloth render mesh. */
	UPROPERTY(EditAnywhere, Category = RayTracing)
	bool bSmoothTransition = true;

	/** Whether to use multiple triangle influences on the proxy wrap deformer to help smoothen deformations. */
	UPROPERTY(EditAnywhere, Category = RayTracing)
	bool bUseMultipleInfluences = false;

	/** The radius from which to get the multiple triangle influences from the simulated proxy mesh. */
	UPROPERTY(EditAnywhere, Category = RayTracing)
	float SkinningKernelRadius = 30.f;

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

	/** Cloth Collection containing this asset data. */
	TSharedPtr<UE::Chaos::ClothAsset::FClothCollection> ClothCollection;

	/** Reference skeleton created from the provided skeleton asset. */
	FReferenceSkeleton RefSkeleton;

	/** Reference skeleton precomputed bases. */
	TArray<FMatrix44f> RefBasesInvMatrix;

	/** Rendering data. */
	TUniquePtr<FSkeletalMeshRenderData> SkeletalMeshRenderData;

	/** A fence which is used to keep track of the rendering thread releasing the static mesh resources. */
	FRenderCommandFence ReleaseResourcesFence;

#if WITH_EDITORONLY_DATA
	/** Source mesh geometry information (not used at runtime). */
	TSharedPtr<FSkeletalMeshModel> MeshModel;
#endif

	/** Simulation mesh Lods as fed to the solver for constraints creation. Ownership gets transferred to the proxy when it is changed during a simulation. */
	TSharedPtr<FChaosClothSimulationModel> ClothSimulationModel;

	friend class UClothAssetBuilderEditor;
};
