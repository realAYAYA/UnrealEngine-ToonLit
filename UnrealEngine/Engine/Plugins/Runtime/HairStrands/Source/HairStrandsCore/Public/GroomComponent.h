// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/MeshComponent.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "RHIDefinitions.h"
#include "GroomDesc.h"
#include "LODSyncInterface.h"
#include "GroomInstance.h"
#include "NiagaraDataInterfacePhysicsAsset.h"

#include "GroomComponent.generated.h"

class UGroomCache;

UCLASS(HideCategories = (Object, Physics, Activation, Mobility, "Components|Activation"), editinlinenew, meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
class HAIRSTRANDSCORE_API UGroomComponent : public UMeshComponent, public ILODSyncInterface, public INiagaraPhysicsAssetDICollectorInterface
{
	GENERATED_UCLASS_BODY()

public:

	/** Groom asset . */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category = "Groom")
	TObjectPtr<UGroomAsset> GroomAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category = "GroomCache", meta = (EditCondition = "BindingAsset == nullptr"))
	TObjectPtr<UGroomCache> GroomCache;

	/** Niagara components that will be attached to the system*/
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UNiagaraComponent>> NiagaraComponents;

	// Kept for debugging mesh transfer
	UPROPERTY()
	TObjectPtr<class USkeletalMesh> SourceSkeletalMesh;

	/** Optional binding asset for binding a groom onto a skeletal mesh. If the binding asset is not provided the projection is done at runtime, which implies a large GPU cost at startup time. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = "Groom", meta = (EditCondition = "GroomCache == nullptr"))
	TObjectPtr<class UGroomBindingAsset> BindingAsset;

	/** Physics asset to be used for hair simulation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation")
	TObjectPtr<class UPhysicsAsset> PhysicsAsset;

	/** List of collision components to be used */
	TArray<TWeakObjectPtr<class USkeletalMeshComponent>> CollisionComponents;
	
	/** Groom's simulation settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category = "Simulation")
	FHairSimulationSettings SimulationSettings;

	/* Reference of the default/debug materials for each geometric representation */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Strands_DebugMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Strands_DefaultMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Cards_DefaultMaterial;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Meshes_DefaultMaterial;

	UPROPERTY()
	TObjectPtr<class UNiagaraSystem> AngularSpringsSystem;

	UPROPERTY()
	TObjectPtr<class UNiagaraSystem> CosseratRodsSystem;

	/** Optional socket name, where the groom component should be attached at, when parented with a skeletal mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, interp, Category = "Groom")
	FString AttachmentName;

	/** Boolean to check when the simulation should be reset */
	bool bResetSimulation;

	/** Boolean to check when the simulation should be initialized */
	bool bInitSimulation;

	/** Previous bone matrix to compare the difference and decide to reset or not the simulation */
	FMatrix	PrevBoneMatrix;

	/** Update Niagara components */
	void UpdateHairSimulation();

	/** Release Niagara components */
	void ReleaseHairSimulation();
	void ReleaseHairSimulation(const int32 GroupIndex);

	/** Create per Group/LOD the Niagara component */
	void CreateHairSimulation(const int32 GroupIndex, const int32 LODIndex);

	/** Enable/Disable hair simulation while transitioning from one LOD to another one */
	void SwitchSimulationLOD(const int32 PreviousLOD, const int32 CurrentLOD);

	/** Check if the simulation is enabled or not */
	bool IsSimulationEnable(int32 GroupIndex, int32 LODIndex) const;

	/** Update Group Description */
	void UpdateHairGroupsDesc();
	void UpdateHairGroupsDescAndInvalidateRenderState(bool bInvalidate=true);

	/** Update simulated groups */
	void UpdateSimulatedGroups();

	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	uint32 GetResourcesSize() const; 
	//~ End UObject Interface.

	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual void OnAttachmentChanged() override;
	virtual void DetachFromComponent(const FDetachmentTransformRules& DetachmentRules) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	//~ End UActorComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual void PostLoad() override;
	virtual void PrecachePSOs() override;
	//~ End UMeshComponent Interface.

	/** Return the guide hairs rest resources*/
	FHairStrandsRestResource* GetGuideStrandsRestResource(uint32 GroupIndex);

	/** Return the guide hairs deformed resources*/
	FHairStrandsDeformedResource* GetGuideStrandsDeformedResource(uint32 GroupIndex);

	/** Return the guide hairs root resources*/
	FHairStrandsRestRootResource* GetGuideStrandsRestRootResource(uint32 GroupIndex);
	FHairStrandsDeformedRootResource* GetGuideStrandsDeformedRootResource(uint32 GroupIndex);


#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	void ValidateMaterials(bool bMapCheck) const;
	void Invalidate();
	void InvalidateAndRecreate();

	void SetDebugMode(EHairStrandsDebugMode InMode);
	EHairStrandsDebugMode GetDebugMode() const { return DebugMode; }
#endif

	/* Accessor function for changing Groom asset from blueprint/sequencer */
	UFUNCTION(BlueprintCallable, Category = "Groom")
	void SetGroomAsset(UGroomAsset* Asset);

	/* Accessor function for changing Groom binding asset from blueprint/sequencer */
	UFUNCTION(BlueprintCallable, Category = "Groom")
	void SetBindingAsset(UGroomBindingAsset* InBinding);

	/* Accessor function for changing Groom physics asset from blueprint/sequencer */
	UFUNCTION(BlueprintCallable, Category = "Simulation")
	void SetPhysicsAsset(UPhysicsAsset* InPhysicsAsset);

	/* Add a skeletal mesh to the collision components */
	UFUNCTION(BlueprintCallable, Category = "Simulation")
	void AddCollisionComponent(USkeletalMeshComponent* SkeletalMeshComponent);
	
	/* Reset the collision components */
	UFUNCTION(BlueprintCallable, Category = "Simulation")
	void ResetCollisionComponents();

	/* Accessor function for changing the enable simulation flag from blueprint/sequencer */
	UFUNCTION(BlueprintCallable, Category = "Simulation")
	void SetEnableSimulation(bool bInEnableSimulation);

	/* Reset the simulation, if enabled */
	UFUNCTION(BlueprintCallable, Category = "Simulation")
	void ResetSimulation();
	
	/* Given the group index return the matching niagara component */
	UFUNCTION(BlueprintCallable, Category = "Simulation")
	UNiagaraComponent* GetNiagaraComponent(const int32 GroupIndex) {return NiagaraComponents.IsValidIndex(GroupIndex) ? NiagaraComponents[GroupIndex] : nullptr;}

	/* Accessor function for changing hair length scale from blueprint/sequencer */
	UFUNCTION(BlueprintCallable, Category = "Groom")
	void SetHairLengthScale(float Scale);

	UFUNCTION(BlueprintCallable, Category = "Groom")
	void SetHairLengthScaleEnable(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Groom")
	bool GetIsHairLengthScaleEnabled();

	void SetStableRasterization(bool bEnable);
	void SetGroomAsset(UGroomAsset* Asset, UGroomBindingAsset* InBinding, const bool bUpdateSimulation = true);
	void SetHairRootScale(float Scale);
	void SetHairWidth(float HairWidth);
	void SetScatterSceneLighting(bool Enable);
	void SetBinding(UGroomBindingAsset* InBinding);
	void SetUseCards(bool InbUseCards);
	void SetValidation(bool bEnable) { bValidationEnable = bEnable; }

	///~ Begin ILODSyncInterface Interface.
	virtual int32 GetDesiredSyncLOD() const override;
	virtual void SetSyncLOD(int32 LODIndex) override;
	virtual int32 GetNumSyncLODs() const override;
	virtual int32 GetCurrentSyncLOD() const override;
	//~ End ILODSyncInterface

	int32 GetNumLODs() const;
	int32 GetForcedLOD() const;
	void  SetForcedLOD(int32 LODIndex);

	/** Groom's groups info. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom")
	TArray<FHairGroupDesc> GroomGroupsDesc;

	/** Force the groom to use cards/meshes geometry instead of strands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Groom")
	bool bUseCards = false;

	/** Hair group instance access */
	uint32 GetGroupCount() const { return HairGroupInstances.Num();  }
	FHairGroupInstance* GetGroupInstance(int32 Index) { return Index >= 0 && Index < HairGroupInstances.Num() ? HairGroupInstances[Index] : nullptr; }
	const FHairGroupInstance* GetGroupInstance(int32 Index) const { return Index >= 0 && Index < HairGroupInstances.Num() ? HairGroupInstances[Index] : nullptr; }
	bool ContainsGroupInstance(FHairGroupInstance* Instance) const { return HairGroupInstances.Contains(Instance); }

	//~ Begin UPrimitiveComponent Interface
	EHairGeometryType GetMaterialGeometryType(int32 ElementIndex) const;
	UMaterialInterface* GetMaterial(int32 ElementIndex, EHairGeometryType GeometryType, bool bUseDefaultIfIncompatible) const;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual int32 GetMaterialIndex(FName MaterialSlotName) const override;
	virtual TArray<FName> GetMaterialSlotNames() const override;
	virtual bool IsMaterialSlotNameValid(FName MaterialSlotName) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual int32 GetNumMaterials() const override;
	//~ End UPrimitiveComponent Interface

#if WITH_EDITORONLY_DATA
	// Set the component in preview mode, forcing the loading of certain data
	void SetPreviewMode(bool bValue) { bPreviewMode = bValue; };
#endif

	/** GroomCache */
	UGroomCache* GetGroomCache() const { return GroomCache; }
	void SetGroomCache(UGroomCache* InGroomCache);

	float GetGroomCacheDuration() const;

	void SetManualTick(bool bInManualTick);
	bool GetManualTick() const;
	void TickAtThisTime(const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping);

	void ResetAnimationTime();
	float GetAnimationTime() const;
	bool IsLooping() const { return bLooping; }

	/** Build the local simulation transform that could be used in strands simulation */
	void BuildSimulationTransform(FTransform& SimulationTransform) const;

	//~ Begin INiagaraPhysicsAssetDICollectorInterface Interface
	virtual UPhysicsAsset* BuildAndCollect(FTransform& BoneTransform, TArray<TWeakObjectPtr<USkeletalMeshComponent>>& SourceComponents, TArray<TWeakObjectPtr<UPhysicsAsset>>& PhysicsAssets) const override;
	//~ End INiagaraPhysicsAssetDICollectorInterface Interface

private:
	void UpdateGroomCache(float Time);

	UPROPERTY(EditAnywhere, Category = GroomCache)
	bool bRunning;

	UPROPERTY(EditAnywhere, Category = GroomCache)
	bool bLooping;

	UPROPERTY(EditAnywhere, Category = GroomCache)
	bool bManualTick;

	UPROPERTY(VisibleAnywhere, transient, Category = GroomCache)
	float ElapsedTime;

	TSharedPtr<class IGroomCacheBuffers, ESPMode::ThreadSafe> GroomCacheBuffers;

private:
	TArray<FHairGroupInstance*> HairGroupInstances;
	TArray<FHairGroupInstance*> DeferredDeleteHairGroupInstances;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<UGroomAsset> GroomAssetBeingLoaded;

	UPROPERTY(Transient)
	TObjectPtr<UGroomBindingAsset> BindingAssetBeingLoaded;
#endif

protected:
	// Used for tracking if a Niagara component is attached or not
	virtual void OnChildAttached(USceneComponent* ChildComponent) override;
	virtual void OnChildDetached(USceneComponent* ChildComponent) override;

private:
	void DeleteDeferredHairGroupInstances();
	void* InitializedResources;
	class UMeshComponent* RegisteredMeshComponent;
	class UMeshComponent* DeformedMeshComponent;
	FVector SkeletalPreviousPositionOffset;
	bool bIsGroomAssetCallbackRegistered;
	bool bIsGroomBindingAssetCallbackRegistered;
	bool bValidationEnable = true;
	bool bPreviewMode = false;

	// LOD selection
	EHairLODSelectionType LODSelectionType = EHairLODSelectionType::Immediate;
	float LODPredictedIndex = -1.f;
	float LODForcedIndex = -1.f;

	// Transient property for visualizing the groom in a certain debug mode. This is used by the groom editor
	EHairStrandsDebugMode DebugMode = EHairStrandsDebugMode::NoneDebug;

	void InitResources(bool bIsBindingReloading=false);
	void ReleaseResources();

	friend class FGroomComponentRecreateRenderStateContext;
	friend class FHairStrandsSceneProxy;
	friend class FHairCardsSceneProxy;
};

#if WITH_EDITORONLY_DATA
/** Used to recreate render context for all GroomComponents that use a given GroomAsset */
class HAIRSTRANDSCORE_API FGroomComponentRecreateRenderStateContext
{
public:
	FGroomComponentRecreateRenderStateContext(UGroomAsset* GroomAsset);
	~FGroomComponentRecreateRenderStateContext();

private:
	TArray<UGroomComponent*> GroomComponents;
};

/** Return the debug color of an hair group */
HAIRSTRANDSCORE_API const FLinearColor GetHairGroupDebugColor(int32 GroupIt);

#endif

