// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SkinnedMeshComponent.h"

#include "ClothComponent.generated.h"

class UChaosClothAsset;
class UChaosClothComponent;

namespace UE::Chaos::ClothAsset
{
	class FClothSimulationProxy;
}
/**
 * Cloth simulation component.
 */
UCLASS(
	ClassGroup = Physics, 
	Meta = (BlueprintSpawnableComponent, ToolTip = "Chaos cloth simulation component."),
	DisplayName = "Cloth Simulation",
	HideCategories = (Object, "Mesh|SkeletalAsset", Constraints, Advanced, Cooking, Collision, Navigation))
class CHAOSCLOTHASSETENGINE_API UChaosClothComponent : public USkinnedMeshComponent
{
	GENERATED_BODY()
public:
	UChaosClothComponent(const FObjectInitializer& ObjectInitializer);
	UChaosClothComponent(FVTableHelper& Helper);
	~UChaosClothComponent();

	/** Set the cloth asset used by this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|ClothAsset", Meta = (Keywords = "Chaos Cloth Asset"))
	void SetClothAsset(UChaosClothAsset* InClothAsset);

	/** Get the cloth asset used by this component. */
	UFUNCTION(BlueprintPure, Category = "Components|ClothAsset", Meta = (Keywords = "Chaos Cloth Asset"))
	UChaosClothAsset* GetClothAsset() const;

	/** Reset the teleport mode. */
	UFUNCTION(BlueprintCallable, Category = "Components|Teleport", Meta = (Keywords = "Chaos Cloth Teleport"))
	void ResetTeleportMode() { bTeleport = bReset = false; }

	/** Teleport the cloth particles to the new reference bone location keeping pose and velocities prior to advancing the simulation. */
	UFUNCTION(BlueprintCallable, Category = "Components||Teleport", Meta = (Keywords = "Chaos Cloth Teleport"))
	void ForceNextUpdateTeleport() { bTeleport = true; bReset = false; }

	/** Teleport the cloth particles to the new reference bone location while reseting the pose and velocities prior to advancing the simulation. */
	UFUNCTION(BlueprintCallable, Category = "Components|Simulation", Meta = (Keywords = "Chaos Cloth Teleport Reset"))
	void ForceNextUpdateTeleportAndReset() { bTeleport = bReset = true; }

	/** Return whether teleport is currently requested. */
	bool NeedsTeleport() const { return bTeleport; }

	/** Return whether reseting the pose is currently requested. */
	bool NeedsReset() const { return bReset; }

	/** Stop the simulation, and keep the cloth in its last pose. */
	UFUNCTION(BlueprintCallable, Category = "Components|Simulation", Meta = (UnsafeDuringActorConstruction, Keywords = "Chaos Cloth Simulation Suspend"))
	void SuspendSimulation() { bSuspendSimulation = true; }

	/** Resume a previously suspended simulation. */
	UFUNCTION(BlueprintCallable, Category = "Components|Simulation", Meta = (UnsafeDuringActorConstruction, Keywords = "Chaos Cloth Simulation Resume"))
	void ResumeSimulation() { bSuspendSimulation = false; }

	/** Return whether or not the simulation is currently suspended. */
	UFUNCTION(BlueprintCallable, Category = "Components|Simulation", Meta = (Keywords = "Chaos Cloth Simulation Suspend"))
	bool IsSimulationSuspended() const;

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
	//~ End USkinnedMeshComponent Interface

private:
	void StartNewParallelSimulation(float DeltaTime);
	void HandleExistingParallelSimulation();
	bool ShouldWaitForParallelSimulationInTickComponent() const;

#if WITH_EDITORONLY_DATA
	/** Cloth asset used by this component. */
	UE_DEPRECATED(5.1, "This property isn't deprecated, but getter and setter must be used at all times to preserve correct operations.")
	UPROPERTY(EditAnywhere, Transient, BlueprintSetter = SetClothAsset, BlueprintGetter = GetClothAsset, Category = ClothAsset)
	TObjectPtr<UChaosClothAsset> ClothAsset;
#endif

	/** If enabled, and the parent is another Skinned Mesh Component (e.g. another Cloth Component, Poseable Mesh Component, Skeletal Mesh Component, ...etc.), use its pose. */
	UPROPERTY(EditAnywhere, Category = ClothComponent)
	uint8 bUseAttachedParentAsPoseComponent : 1;

	/** Whether to wait for the cloth simulation to end in the TickComponent instead of in the EndOfFrameUpdates. */
	UPROPERTY(EditAnywhere, Category = ClothComponent)
	uint8 bWaitForParallelTask : 1;

	/** Whether to disable the simulation and use the skinned pose instead. */
	UPROPERTY()
	uint8 bDisableSimulation : 1;

	/** Whether to suspend the simulation and use the last simulated pose. */
	UPROPERTY()
	uint8 bSuspendSimulation : 1;


	/** Whether to use the leader component pose. */
	UPROPERTY()
	uint8 bBindToLeaderComponent : 1;

	/** Whether to teleport the cloth prior to advancing the simulation. */
	UPROPERTY()
	uint8 bTeleport : 1;

	/** Whether to reset the pose, bTeleport must be true. */
	UPROPERTY()
	uint8 bReset : 1;

	/** Blend amount between the skinned (=0) and the simulated pose (=1). */
	UPROPERTY()
	float BlendWeight = 1.f;

	TUniquePtr<UE::Chaos::ClothAsset::FClothSimulationProxy> ClothSimulationProxy;
};
