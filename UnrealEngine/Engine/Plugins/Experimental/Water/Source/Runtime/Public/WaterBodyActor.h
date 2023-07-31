// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Scene.h"
#include "TerrainCarvingSettings.h"
#include "WaterBrushActorInterface.h"
#include "WaterBodyWeightmapSettings.h"
#include "WaterBodyHeightmapSettings.h"
#include "WaterCurveSettings.h"
#include "WaterSplineMetadata.h"
#include "NavAreas/NavArea.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "WaterBodyTypes.h"
#include "WaterWaves.h"
#include "WaterBodyComponent.h"
#include "WaterBodyActor.generated.h"

class UWaterSplineComponent;
class AWaterBodyIsland;
class AWaterBodyExclusionVolume;
class ALandscapeProxy;
class UMaterialInstanceDynamic;

// ----------------------------------------------------------------------------------

// For internal use.
UCLASS(Abstract, Deprecated, Within = WaterBody)
class WATER_API UDEPRECATED_WaterBodyGenerator : public UObject
{
	GENERATED_UCLASS_BODY()
};

// ----------------------------------------------------------------------------------

//@todo_water: Remove Blueprintable
UCLASS(Blueprintable, Abstract, HideCategories = (Tags, Activation, Cooking, Replication, Input, Actor, AssetUserData))
class WATER_API AWaterBody : public AActor, public IWaterBrushActorInterface
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin IWaterBrushActorInterface interface
	virtual bool AffectsLandscape() const override { return WaterBodyComponent->AffectsLandscape(); }
	virtual bool AffectsWaterMesh() const override { return WaterBodyComponent->AffectsWaterMesh(); }
	virtual bool CanEverAffectWaterMesh() const override { return WaterBodyComponent->CanEverAffectWaterMesh(); }

#if WITH_EDITOR
	virtual const FWaterCurveSettings& GetWaterCurveSettings() const override { return WaterBodyComponent->GetWaterCurveSettings(); }
	virtual const FWaterBodyHeightmapSettings& GetWaterHeightmapSettings() const override { return WaterBodyComponent->GetWaterHeightmapSettings(); }
	virtual const TMap<FName, FWaterBodyWeightmapSettings>& GetLayerWeightmapSettings() const override { return WaterBodyComponent->GetLayerWeightmapSettings(); }
	virtual ETextureRenderTargetFormat GetBrushRenderTargetFormat() const override { return WaterBodyComponent->GetBrushRenderTargetFormat(); }
	virtual void GetBrushRenderDependencies(TSet<UObject*>& OutDependencies) const override { return WaterBodyComponent->GetBrushRenderDependencies(OutDependencies); }
	virtual TArray<UPrimitiveComponent*> GetBrushRenderableComponents() const override { return WaterBodyComponent->GetBrushRenderableComponents(); }
#endif //WITH_EDITOR
	//~ End IWaterBrushActorInterface interface

	/** Actor Interface */
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;
	virtual void PreRegisterAllComponents() override;
	virtual void UnregisterAllComponents(bool bForReregister = false) override;
	virtual void PreInitializeComponents() override;
	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostRegisterAllComponents() override;
	virtual bool IsHLODRelevant() const override;

#if WITH_EDITOR
	virtual void SetActorHiddenInGame(bool bNewHidden) override;
	virtual void SetIsTemporarilyHiddenInEditor(bool bIsHidden) override;
	virtual bool SetIsHiddenEdLayer(bool bIsHiddenEdLayer) override;
	virtual void GetActorDescProperties(FPropertyPairsMap& PropertyPairsMap) const override;
#endif // WITH_EDITOR
	
	/** Returns the type of body */
	UFUNCTION(BlueprintCallable, Category=Water)
	virtual EWaterBodyType GetWaterBodyType() const { return IsTemplate() ? WaterBodyType : GetClass()->GetDefaultObject<AWaterBody>()->WaterBodyType; }
	
	/** Returns water spline component */
	UFUNCTION(BlueprintCallable, Category=Water)
	UWaterSplineComponent* GetWaterSpline() const { return SplineComp; }

	UWaterSplineMetadata* GetWaterSplineMetadata() { return WaterSplineMetadata; }

	const UWaterSplineMetadata* GetWaterSplineMetadata() const { return WaterSplineMetadata; }

	UFUNCTION(BlueprintCallable, Category = Wave)
	void SetWaterWaves(UWaterWavesBase* InWaterWaves);
	UWaterWavesBase* GetWaterWaves() const { return WaterWaves; }

	/** Returns the water body component */
	UFUNCTION(BlueprintCallable, Category=Water)
	UWaterBodyComponent* GetWaterBodyComponent() const { return WaterBodyComponent; }

protected:
	/** Initializes the water body by creating the respective component for this water body type. */
	virtual void InitializeBody();

	virtual void DeprecateData();

	/** The spline data attached to this water type. */
	UPROPERTY(Category = Water, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWaterSplineComponent> SplineComp;

	UPROPERTY(Instanced)
	TObjectPtr<UWaterSplineMetadata> WaterSplineMetadata;

	UPROPERTY(Category = Water, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Water,Wave,Rendering,Terrain,Navigation,Physics,Collision,Debug", AllowPrivateAccess = "true"))
	TObjectPtr<UWaterBodyComponent> WaterBodyComponent;

	/** Unique Id for accessing (wave, ... ) data in GPU buffers */
	UPROPERTY(Transient, DuplicateTransient, NonTransactional, BlueprintReadOnly, Category = Water, meta = (AllowPrivateAccess = "true"))
	int32 WaterBodyIndex = INDEX_NONE;
	
	UPROPERTY(Category = Water, EditDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	EWaterBodyType WaterBodyType;

	// #todo_water: This should be moved to the component when component subobjects are supported
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = Wave, DisplayName = "Waves Source", meta = (Tooltip = ""))
	TObjectPtr<UWaterWavesBase> WaterWaves = nullptr;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PreEditChange(FProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif // WITH_EDITOR

	void SetWaterWavesInternal(UWaterWavesBase* InWaterWaves);

// ----------------------------------------------------------------------------------
// Deprecated

public:
	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = Rendering, meta = (DeprecatedFunction))
	virtual UMaterialInstanceDynamic* GetRiverToLakeTransitionMaterialInstance() final { return WaterBodyComponent->GetRiverToLakeTransitionMaterialInstance(); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = Rendering, meta = (DeprecatedFunction))
	virtual UMaterialInstanceDynamic* GetRiverToOceanTransitionMaterialInstance() final { return WaterBodyComponent->GetRiverToOceanTransitionMaterialInstance(); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = Rendering, meta = (DeprecatedFunction))
	void SetWaterMaterial(UMaterialInterface* InMaterial) { WaterBodyComponent->SetWaterMaterial(InMaterial); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = Rendering, meta = (DeprecatedFunction))
	UMaterialInstanceDynamic* GetWaterMaterialInstance() { return WaterBodyComponent->GetWaterMaterialInstance(); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = WaterBody, meta = (DeprecatedFunction))
	virtual float GetWaterVelocityAtSplineInputKey(float InKey) const { return WaterBodyComponent->GetWaterVelocityAtSplineInputKey(InKey); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = WaterBody, meta = (DeprecatedFunction))
	virtual FVector GetWaterVelocityVectorAtSplineInputKey(float InKey) const { return WaterBodyComponent->GetWaterVelocityVectorAtSplineInputKey(InKey); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = WaterBody, meta = (DeprecatedFunction))
	virtual float GetAudioIntensityAtSplineInputKey(float InKey) const { return WaterBodyComponent->GetAudioIntensityAtSplineInputKey(InKey); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = Water, meta = (DeprecatedFunction))
	TArray<AWaterBodyIsland*> GetIslands() const { return WaterBodyComponent->GetIslands(); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category = Water, meta = (DeprecatedFunction))
	TArray<AWaterBodyExclusionVolume*> GetExclusionVolumes() const { return WaterBodyComponent->GetExclusionVolumes(); }

	UE_DEPRECATED(4.27, "Moved to WaterBodyComponent")
	UFUNCTION(BlueprintCallable, Category=Water, meta = (DeprecatedFunction))
	void OnWaterBodyChanged(bool bShapeOrPositionChanged, bool bWeightmapSettingsChanged = false) { return WaterBodyComponent->OnWaterBodyChanged(bShapeOrPositionChanged, bWeightmapSettingsChanged); }

#if WITH_EDITOR
	UE_DEPRECATED(5.1, "Moved to WaterBodyComponent")
	virtual bool IsIconVisible() const { return WaterBodyComponent->IsIconVisible(); }
#endif // WITH_EDITOR
protected:
	friend class UWaterBodyComponent;
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UPhysicalMaterial> PhysicalMaterial_DEPRECATED;

	UPROPERTY()
	float TargetWaveMaskDepth_DEPRECATED;

	UPROPERTY()
	float MaxWaveHeightOffset_DEPRECATED = 0.f;

	UPROPERTY()
	bool bFillCollisionUnderWaterBodiesForNavmesh_DEPRECATED;

	UPROPERTY()
	FUnderwaterPostProcessSettings UnderwaterPostProcessSettings_DEPRECATED;

	UPROPERTY()
	FWaterCurveSettings CurveSettings_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> WaterMaterial_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> UnderwaterPostProcessMaterial_DEPRECATED;

	UPROPERTY()
	FLandmassTerrainCarvingSettings TerrainCarvingSettings_DEPRECATED;

	UPROPERTY()
	FWaterBodyHeightmapSettings WaterHeightmapSettings_DEPRECATED;

	UPROPERTY()
	TMap<FName, FWaterBodyWeightmapSettings> LayerWeightmapSettings_DEPRECATED;

	UPROPERTY()
	bool bAffectsLandscape_DEPRECATED;
	
	UPROPERTY()
	bool bGenerateCollisions_DEPRECATED = true;

	UPROPERTY()
	bool bOverrideWaterMesh_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UStaticMesh> WaterMeshOverride_DEPRECATED;

	UPROPERTY()
	int32 OverlapMaterialPriority_DEPRECATED = 0;

	UPROPERTY()
	FName CollisionProfileName_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> WaterMID_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> UnderwaterPostProcessMID_DEPRECATED;

	UPROPERTY()
	TArray<TLazyObjectPtr<AWaterBodyIsland>> Islands_DEPRECATED;

	UPROPERTY()
	TArray<TLazyObjectPtr<AWaterBodyExclusionVolume>> ExclusionVolumes_DEPRECATED;

	UPROPERTY()
	bool bCanAffectNavigation_DEPRECATED;

	UPROPERTY()
	TSubclassOf<UNavAreaBase> WaterNavAreaClass_DEPRECATED;

	UPROPERTY()
	float ShapeDilation_DEPRECATED = 4096.0f;
#endif // WITH_EDITORONLY_DATA
};
