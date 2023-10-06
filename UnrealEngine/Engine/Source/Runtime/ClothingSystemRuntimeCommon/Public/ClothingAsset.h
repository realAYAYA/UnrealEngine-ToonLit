// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClothingAssetBase.h"
#include "ClothConfigBase.h"
#include "ClothLODData.h"
#include "ClothConfig_Legacy.h"
#include "ClothLODData_Legacy.h"
#include "ClothingSimulationInteractor.h"
#include "Algo/AnyOf.h"
#include "ClothingAsset.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogClothingAsset, Log, All);

class UPhysicsAsset;
struct FSkelMeshSection;

namespace ClothingAssetUtils
{
	/**
	 * Helper struct to hold binding information on a clothing asset, used to 
	 * enumerate all of the bindings on a skeletal mesh with 
	 * \c GetAllMeshClothingAssetBindings() below.
	 */
	struct FClothingAssetMeshBinding
	{
		UClothingAssetCommon* Asset;
		int32 LODIndex;
		int32 SectionIndex;
		int32 AssetInternalLodIndex;
	};

	/**
	 * Given a skeletal mesh, find all of the currently bound clothing assets and their binding information
	 * @param InSkelMesh - The skeletal mesh to search
	 * @param OutBindings - The list of bindings to write to
	 */
	UE_DEPRECATED(5.0, "This Handler Is Now Deprecated, use GetAllMeshClothingAssetBindings instead.")
	void CLOTHINGSYSTEMRUNTIMECOMMON_API 
	GetMeshClothingAssetBindings(
		USkeletalMesh* InSkelMesh, 
		TArray<FClothingAssetMeshBinding>& OutBindings);
	
	/**
	 * Similar to above, but only inspects the specified LOD.
	 */
	UE_DEPRECATED(5.0, "This Handler Is Now Deprecated, use GetAllLodMeshClothingAssetBindings instead.")
	void CLOTHINGSYSTEMRUNTIMECOMMON_API 
	GetMeshClothingAssetBindings(
		USkeletalMesh* InSkelMesh, 
		TArray<FClothingAssetMeshBinding>& OutBindings, 
		int32 InLodIndex);

#if WITH_EDITOR
	/**
	 * Given a skeletal mesh model, find all of the currently bound clothing assets and their binding information
	 * @param MeshModel - The skeletal mesh imported model to extract binding from
	 * @param OutBindings - The list of bindings to write to
	 */
	void CLOTHINGSYSTEMRUNTIMECOMMON_API
	GetAllMeshClothingAssetBindings(
		const USkeletalMesh* InSkelMesh,
		TArray<FClothingAssetMeshBinding>& OutBindings);

	/**
	 * Similar to above, but only inspects the specified LOD.
	 */
	void CLOTHINGSYSTEMRUNTIMECOMMON_API
	GetAllLodMeshClothingAssetBindings(
		const USkeletalMesh* InSkelMesh,
		TArray<FClothingAssetMeshBinding>& OutBindings,
		int32 InLodIndex);

	/**
	 * Clears the clothing tracking struct of a section.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API void 
	ClearSectionClothingData(FSkelMeshSection& InSection);
#endif
}

/**
 * Custom data wrapper for clothing assets.
 */
class UE_DEPRECATED(5.0, "Redundant class.") UClothingAssetCustomData;
UCLASS(Abstract, MinimalAPI)
class UClothingAssetCustomData : public UObject
{
	GENERATED_BODY()
public:
	virtual void BindToSkeletalMesh(USkeletalMesh* InSkelMesh, int32 InMeshLodIndex, int32 InSectionIndex, int32 InAssetLodIndex)
	{}
};

/**
 * Common flags used by InvalidateCachedData.
 */
enum class EClothingCachedDataFlagsCommon : uint8
{
	None = 0,
	InverseMasses = 1 << 0,
	NumInfluences = 1 << 1,
	SelfCollisionData = 1 << 2,
	Tethers = 1 << 3,
	All = 0xFF
};
ENUM_CLASS_FLAGS(EClothingCachedDataFlagsCommon);

/**
 * Implementation of non-solver specific, but common Engine related functionality.
 *
 * Solver specific implementations may wish to override this class to construct
 * their own default instances of child classes, such as \c ClothSimConfig and 
 * \c CustomData, as well as override the \c AddNewLod() factory to build their 
 * own implementation of \c UClothLODDataBase.
 */
UCLASS(hidecategories = Object, BlueprintType, MinimalAPI)
class UClothingAssetCommon : public UClothingAssetBase
{
	GENERATED_BODY()
public:

	CLOTHINGSYSTEMRUNTIMECOMMON_API UClothingAssetCommon(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR

	/**
	 * Create weights for skinning the render mesh to our simulation mesh, and 
	 * weights to drive our sim mesh from the skeleton.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual bool BindToSkeletalMesh(USkeletalMesh* InSkelMesh, const int32 InMeshLodIndex, const int32 InSectionIndex, const int32 InAssetLodIndex) override;

	/**
	 * Helper that invokes \c UnbindFromSkeletalMesh() for each avilable entry in 
	 * \p InSkelMesh->GetImportedModel()'s LODModel.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void UnbindFromSkeletalMesh(USkeletalMesh* InSkelMesh) override;
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void UnbindFromSkeletalMesh(USkeletalMesh* InSkelMesh, const int32 InMeshLodIndex) override;

	/**
	 * Update all extra LOD deformer mappings.
	 * This should be called whenever the raytracing LOD bias has changed.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void UpdateAllLODBiasMappings(USkeletalMesh* SkeletalMesh) override;

	/** 
	 * Callback envoked after weights have been edited.
	 * Calls \c PushWeightsToMesh() on each \c ClothLodData, and invalidates cached data. 
	 * Optionaly recalculate the owner's sections fixed vertex data based on this asset masks, and invalidate the DDC for this asset.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API void ApplyParameterMasks(bool bUpdateFixedVertData = false, bool bInvalidateDerivedDataCache = true);

	/**
	 * Builds the LOD transition data.
	 * When we transition between LODs we skin the incoming mesh to the outgoing mesh
	 * in exactly the same way the render mesh is skinned to create a smooth swap
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API void BuildLodTransitionData();

	//~ Begin UObject interface
	/**
	 * Stop any simulation from using this asset.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void PreEditUndo() override;

	/**
	 * Restart simulation using this asset after undo change.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void PostEditUndo() override;
	//~ End UObject interface
#endif // WITH_EDITOR

	/**
	 * Rebuilds the \c UsedBoneIndices array from looking up the entries in the
	 * \c UsedBoneNames array, in the \p InSkelMesh's reference skeleton.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void RefreshBoneMapping(USkeletalMesh* InSkelMesh) override;

	/** Calculates the preferred root bone for the simulation. */
	CLOTHINGSYSTEMRUNTIMECOMMON_API void CalculateReferenceBoneIndex();

	/** Returns \c true if \p InLodIndex is a valid LOD id (index into \c ClothLodData). */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual bool IsValidLod(int32 InLodIndex) const override;

	/** Returns the number of valid LOD's (length of the \c ClothLodData array). */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual int32 GetNumLods() const override;

#if WITH_EDITORONLY_DATA
	/**
	 * Called on the clothing asset when the base data (physical mesh, config etc.)
	 * has changed, so any intermediate generated data can be regenerated.
	 */
	virtual void InvalidateAllCachedData() override { InvalidateFlaggedCachedData(EClothingCachedDataFlagsCommon::All); }

	/**
	 * Called on the clothing asset when the base data (physical mesh, config etc.)
	 * has changed, so any intermediate generated data can be regenerated.
	 */
	CLOTHINGSYSTEMRUNTIMECOMMON_API void InvalidateFlaggedCachedData(EClothingCachedDataFlagsCommon Flags);
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	/** * Add a new LOD class instance. */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual int32 AddNewLod() override;

	/* Called after changes in any of the asset properties. */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& ChainEvent) override;
#endif // WITH_EDITOR

	/**
	 * Builds self collision data.
	 * The default behavior is if the \c ClothSimConfig reports that self collisions
	 * are enabled, it envokes \c BuildSelfCollisionData() on each \c ClothLodData's
	 * \c ClothPhysicalMeshData member.
	 */
	UE_DEPRECATED(5.0, "Cached data are now all rebuilt by calling InvalidateCachedData")
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual void BuildSelfCollisionData() override
	{
#if WITH_EDITORONLY_DATA
		InvalidateFlaggedCachedData(EClothingCachedDataFlagsCommon::SelfCollisionData);
#endif // WITH_EDITORONLY_DATA
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Return a const cloth config pointer of the desired cloth config type, or nullptr if there isn't any suitable. */
	template<typename ClothConfigType, typename = typename TEnableIf<TIsDerivedFrom<ClothConfigType, UClothConfigBase>::IsDerived>::Type>
	const ClothConfigType* GetClothConfig() const
	{
		auto const* const ClothConfig = ClothConfigs.Find(ClothConfigType::StaticClass()->GetFName());
		return ClothConfig ? ExactCast<ClothConfigType>(*ClothConfig) : nullptr;
	}

	/** Return a cloth config pointer of the desired cloth config type, or nullptr if there isn't any suitable. */
	template<typename ClothConfigType, typename = typename TEnableIf<TIsDerivedFrom<ClothConfigType, UClothConfigBase>::IsDerived>::Type>
	ClothConfigType* GetClothConfig()
	{
		auto const* const ClothConfig = ClothConfigs.Find(ClothConfigType::StaticClass()->GetFName());
		return ClothConfig ? ExactCast<ClothConfigType>(*ClothConfig) : nullptr;
	}

	/** Migrate deprecated objects. */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static CLOTHINGSYSTEMRUNTIMECOMMON_API void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	/** Serialize deprecated objects. */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void Serialize(FArchive& Ar) override;

	/** Propagate the shared simulation configs between assets. Called after all cloth assets sharing the same simulation are loaded. */
	CLOTHINGSYSTEMRUNTIMECOMMON_API virtual void PostUpdateAllAssets() override;

	// The physics asset to extract collisions from when building a simulation.
	UPROPERTY(EditAnywhere, Category = Config)
	TObjectPtr<UPhysicsAsset> PhysicsAsset;

	// Simulation specific cloth parameters. 
	// Use GetClothConfig() to retrieve the correct parameters/config type for the desired cloth simulation system.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, EditFixedSize, Instanced, Category = Config)
	TMap<FName, TObjectPtr<UClothConfigBase>> ClothConfigs;

#if WITH_EDITORONLY_DATA
	// Shared by all cloth instances in a skeletal mesh
	// Only supported with Chaos Cloth for now
	// This may not be editable on unused cloth assets
	UPROPERTY()
	TObjectPtr<UClothConfigBase> ClothSharedSimConfig_DEPRECATED;

	// Parameters for how the NVcloth behaves.
	// These will have no effect on Chaos cloth
	UPROPERTY()
	TObjectPtr<UClothConfigBase> ClothSimConfig_DEPRECATED;

	// Parameters for how Chaos cloth behaves 
	// These will not affect NVcloth
	// For now, we have two configuration parameters so that we can switch between chaos and
	// non chaos at will without losing the original NVcloth data
	UPROPERTY()
	TObjectPtr<UClothConfigBase> ChaosClothSimConfig_DEPRECATED;

	// Deprecated. Use LodData instead
	UPROPERTY()
	TArray<TObjectPtr<UClothLODDataCommon_Legacy>> ClothLodData_DEPRECATED;
#endif

	// The actual asset data, listed by LOD.
	UPROPERTY()
	TArray<FClothLODDataCommon> LodData;

	// Tracks which clothing LOD each skel mesh LOD corresponds to (LodMap[SkelLod]=ClothingLod).
	UPROPERTY()
	TArray<int32> LodMap;

	// List of bones this asset uses inside its parent mesh.
	UPROPERTY()
	TArray<FName> UsedBoneNames;

	// List of the indices for the bones in UsedBoneNames, used for remapping.
	UPROPERTY()
	TArray<int32> UsedBoneIndices;

	// Bone to treat as the root of the simulation space.
	UPROPERTY()
	int32 ReferenceBoneIndex;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Custom data applied by the importer depending on where the asset was imported from.
	UPROPERTY()
	TObjectPtr<UClothingAssetCustomData> CustomData;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_EDITORONLY_DATA
	/** 
	 * Deprecated property for transitioning the \c FClothConfig struct to the 
	 * \c UClothConfigBase array, in a new property called \c ClothConfigs.
	 */
	UPROPERTY()
	FClothConfig_Legacy ClothConfig_DEPRECATED;
#endif

private:

	// Add or replace a new cloth config of the specified type.
	template<typename ClothConfigType, typename = typename TEnableIf<TIsDerivedFrom<ClothConfigType, UClothConfigBase>::IsDerived>::Type>
	void SetClothConfig(ClothConfigType* ClothConfig)
	{
		check(ClothConfig);
		ClothConfigs.Add(ClothConfig->GetClass()->GetFName(), static_cast<UClothConfigBase*>(ClothConfig));
	}

	// Create and add any missing cloth configs.
	// If a config from a different factory exists already, the newly
	// created config will attempt to initialize its parameters from it.
	// Return true when at least one config has been added, false otherwise.
	CLOTHINGSYSTEMRUNTIMECOMMON_API bool AddClothConfigs();

	// Propagate the shared simulation configs between assets.
	// Also migrate all deprecated shared parameters which have been moved to the per cloth configs if required.
	// Called after a cloth asset is created or loaded.
	CLOTHINGSYSTEMRUNTIMECOMMON_API void PropagateSharedConfigs(bool bMigrateSharedConfigToConfig=false);

	// Return true when any one of the cloth configs fullfill the predicate.
	// Used to select which type of data to cache.
	template<typename PredicateType>
	bool AnyOfClothConfigs(PredicateType Predicate) const
	{
		return Algo::AnyOf(ClothConfigs, [&Predicate](const TPair<FName, TObjectPtr<UClothConfigBase>>& ClothConfig)
			{
				return Predicate(*ClothConfig.Value);
			});
	}

#if WITH_EDITOR
	// Add extra cloth deformer mappings to cope with a different raytracing LOD than the one currently rendered.
	CLOTHINGSYSTEMRUNTIMECOMMON_API void UpdateLODBiasMappings(const USkeletalMesh* SkeletalMesh, int32 UpdatedLODIndex, int32 SectionIndex);

	// Clear all defomer section that relies on the specified cloth sim data bound to the specified LOD section 
	CLOTHINGSYSTEMRUNTIMECOMMON_API void ClearLODBiasMappings(const USkeletalMesh* SkeletalMesh, int32 UpdatedLODIndex, int32 SectionIndex);

	// Helper functions used in PostPropertyChangeCb
	CLOTHINGSYSTEMRUNTIMECOMMON_API void ReregisterComponentsUsingClothing();
	CLOTHINGSYSTEMRUNTIMECOMMON_API void ForEachInteractorUsingClothing(TFunction<void (UClothingSimulationInteractor*)> Func);
#endif // WITH_EDITOR
};
