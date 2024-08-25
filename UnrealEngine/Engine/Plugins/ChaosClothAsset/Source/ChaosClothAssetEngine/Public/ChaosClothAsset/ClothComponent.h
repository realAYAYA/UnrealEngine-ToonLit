// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SkinnedMeshComponent.h"
#include "ClothComponent.generated.h"

class UChaosClothAsset;
class UChaosClothComponent;
struct FManagedArrayCollection;

namespace Chaos::Softs
{
	class FCollectionPropertyFacade;
}

namespace UE::Chaos::ClothAsset
{
	class FClothSimulationProxy;
	class FClothComponentCacheAdapter;
}

/**
 * Cloth simulation component.
 */
UCLASS(
	ClassGroup = Physics, 
	Meta = (BlueprintSpawnableComponent, ToolTip = "Chaos cloth component."),
	DisplayName = "Chaos cloth component",
	HideCategories = (Object, "Mesh|SkeletalAsset", Constraints, Advanced, Cooking, Collision, Navigation))
class CHAOSCLOTHASSETENGINE_API UChaosClothComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()	
public:
	UChaosClothComponent(const FObjectInitializer& ObjectInitializer);
	UChaosClothComponent(FVTableHelper& Helper);
	~UChaosClothComponent();

	/** Set the cloth asset used by this component. */
	UFUNCTION(BlueprintCallable, Category = "ClothComponent", Meta = (Keywords = "Chaos Cloth Asset"))
	void SetClothAsset(UChaosClothAsset* InClothAsset);

	/** Get the cloth asset used by this component. */
	UFUNCTION(BlueprintPure, Category = "ClothComponent", Meta = (Keywords = "Chaos Cloth Asset"))
	UChaosClothAsset* GetClothAsset() const;

	/** Reset the teleport mode. */
	UFUNCTION(BlueprintCallable, Category = "ClothComponent", Meta = (Keywords = "Chaos Cloth Teleport"))
	void ResetTeleportMode() { bTeleport = bReset = false; }

	/** Teleport the cloth particles to the new reference bone location keeping pose and velocities prior to advancing the simulation. */
	UFUNCTION(BlueprintCallable, Category = "ClothComponent", Meta = (Keywords = "Chaos Cloth Teleport"))
	void ForceNextUpdateTeleport() { bTeleport = true; bReset = false; }

	/** Teleport the cloth particles to the new reference bone location while reseting the pose and velocities prior to advancing the simulation. */
	UFUNCTION(BlueprintCallable, Category = "ClothComponent", Meta = (Keywords = "Chaos Cloth Teleport Reset"))
	void ForceNextUpdateTeleportAndReset() { bTeleport = bReset = true; }

	/** Return whether teleport is currently requested. */
	bool NeedsTeleport() const { return bTeleport; }

	/** Return whether reseting the pose is currently requested. */
	bool NeedsReset() const { return bReset; }

	/** Stop the simulation, and keep the cloth in its last pose. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "ClothComponent", Meta = (UnsafeDuringActorConstruction, Keywords = "Chaos Cloth Simulation Suspend"))
	void SuspendSimulation() { bSuspendSimulation = true; }

	/** Resume a previously suspended simulation. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "ClothComponent", Meta = (UnsafeDuringActorConstruction, Keywords = "Chaos Cloth Simulation Resume"))
	void ResumeSimulation() { bSuspendSimulation = false; }

	/** Return whether or not the simulation is currently suspended. */
	UFUNCTION(BlueprintCallable, Category = "ClothComponent", Meta = (Keywords = "Chaos Cloth Simulation Suspend"))
	bool IsSimulationSuspended() const;

	/** Set whether or not to enable simulation. */
	UFUNCTION(BlueprintCallable, Category = "ClothComponent", Meta = (UnsafeDuringActorConstruction, Keywords = "Chaos Cloth Simulation Enable"))
	void SetEnableSimulation(bool bEnable) { bEnableSimulation = bEnable; }

	/** Return whether or not the simulation is currently enabled. */
	UFUNCTION(BlueprintCallable, Category = "ClothComponent", Meta = (Keywords = "Chaos Cloth Simulation Enable"))
	bool IsSimulationEnabled() const;

	/** Reset all cloth simulation config properties to the values stored in the original cloth asset. */
	UFUNCTION(BlueprintCallable, Category = "ClothComponent", Meta = (Keywords = "Chaos Cloth Config Property"))
	void ResetConfigProperties();

	/** Hard reset the cloth simulation by recreating the proxy. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "ClothComponent", Meta = (DisplayName = "Hard Reset Simulation", Keywords = "Chaos Cloth Recreate Simulation Proxy"))
	void RecreateClothSimulationProxy();

	/**
	 * Return the property collections holding the runtime properties for this cloth component (one per LOD).
	 * This might be different from the cloth asset's since the component's properties can be modified in code or in blueprints.
	 * This could also be different from the cloth simulation object until the cloth simulation thread synchronise the properties.
	 */
	const TArray<TSharedPtr<const FManagedArrayCollection>>& GetPropertyCollections() const 
	{
		return reinterpret_cast<const TArray<TSharedPtr<const FManagedArrayCollection>>&>(PropertyCollections);
	}

	const UE::Chaos::ClothAsset::FClothSimulationProxy* GetClothSimulationProxy() const { return ClothSimulationProxy.Get(); }

#if WITH_EDITOR
	/** Update config properties from the asset. Will only update existing values.*/
	void UpdateConfigProperties();
#endif

	/** Stalls on any currently running clothing simulations.*/
	void WaitForExistingParallelClothSimulation_GameThread();

#if WITH_EDITOR
	/** This will cause the component to tick once in editor. Both flags will be consumed on that tick. Used for the cache adapter. */
	void SetTickOnceInEditor() { bTickOnceInEditor = true; bTickInEditor = true; }
#endif

protected:
	//~ Begin UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual bool IsComponentTickEnabled() const override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual bool RequiresPreEndOfFrameSync() const override;
	virtual void OnPreEndOfFrameSync() override;
	//~ End UActorComponent Interface

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void OnAttachmentChanged() override;
	//~ End USceneComponent Interface

	//~ Begin USkinnedMeshComponent Interface
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = nullptr) override;
	virtual void GetUpdateClothSimulationData_AnyThread(TMap<int32, FClothSimulData>& OutClothSimulData, FMatrix& OutLocalToWorld, float& OutBlendWeight) override;
	virtual void SetSkinnedAssetAndUpdate(USkinnedAsset* InSkinnedAsset, bool bReinitPose = true) override;
	virtual void GetAdditionalRequiredBonesForLeader(int32 LODIndex, TArray<FBoneIndexType>& InOutRequiredBones) const override;
	//~ End USkinnedMeshComponent Interface

	/** Override this function for setting up custom simulation proxies when the component is registered. */
	virtual TSharedPtr<UE::Chaos::ClothAsset::FClothSimulationProxy> CreateClothSimulationProxy();

private:
	void StartNewParallelSimulation(float DeltaTime);
	void HandleExistingParallelSimulation();
	bool ShouldWaitForParallelSimulationInTickComponent() const;
	void UpdateComponentSpaceTransforms();
	void UpdateVisibility();

	friend UE::Chaos::ClothAsset::FClothComponentCacheAdapter;

#if WITH_EDITORONLY_DATA
	/** Cloth asset used by this component. */
	UE_DEPRECATED(5.1, "This property isn't deprecated, but getter and setter must be used at all times to preserve correct operations.")
	UPROPERTY(EditAnywhere, Transient, Setter = SetClothAsset, BlueprintSetter = SetClothAsset, Getter = GetClothAsset, BlueprintGetter = GetClothAsset, Category = ClothComponent)
	TObjectPtr<UChaosClothAsset> ClothAsset;

	/** Whether to run the simulation in editor. */
	UPROPERTY(EditInstanceOnly, Transient, Category = ClothComponent)
	uint8 bSimulateInEditor : 1;
#endif

	/** If enabled, and the parent is another Skinned Mesh Component (e.g. another Cloth Component, Poseable Mesh Component, Skeletal Mesh Component, ...etc.), use its pose. */
	UPROPERTY(EditAnywhere, Category = ClothComponent)
	uint8 bUseAttachedParentAsPoseComponent : 1;

	/** Whether to wait for the cloth simulation to end in the TickComponent instead of in the EndOfFrameUpdates. */
	UPROPERTY(EditAnywhere, Category = ClothComponent)
	uint8 bWaitForParallelTask : 1;

	/** Whether to enable the simulation or use the skinned pose instead. */
	UPROPERTY(Interp, Category = ClothComponent)
	uint8 bEnableSimulation : 1;

	/** Whether to suspend the simulation and use the last simulated pose. */
	UPROPERTY(Interp, Category = ClothComponent)
	uint8 bSuspendSimulation : 1;

	/** Whether to use the leader component pose. */
	UPROPERTY()
	uint8 bBindToLeaderComponent : 1;

	/** Whether to teleport the cloth prior to advancing the simulation. */
	UPROPERTY(Interp, Category = ClothComponent)
	uint8 bTeleport : 1;

	/** Whether to reset the pose, bTeleport must be true. */
	UPROPERTY(Interp, Category = ClothComponent)
	uint8 bReset : 1;

	/** Blend amount between the skinned (=0) and the simulated pose (=1). */
	UPROPERTY(Interp, Category = ClothComponent)
	float BlendWeight = 1.f;

#if WITH_EDITOR
	bool bTickOnceInEditor = false;
#endif

	TArray<TSharedPtr<FManagedArrayCollection>> PropertyCollections;
	TArray<TUniquePtr<::Chaos::Softs::FCollectionPropertyFacade>> CollectionPropertyFacades;

	TSharedPtr<UE::Chaos::ClothAsset::FClothSimulationProxy> ClothSimulationProxy;
};
