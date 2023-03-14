// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Scene.h"
#include "WaterBrushActorInterface.h"
#include "WaterBodyHeightmapSettings.h"
#include "WaterBodyWeightmapSettings.h"
#include "WaterCurveSettings.h"
#include "WaterBodyIslandActor.generated.h"

class UWaterSplineComponent;
struct FOnWaterSplineDataChangedParams;
class USplineMeshComponent;
class UBillboardComponent;
class AWaterBody;

// ----------------------------------------------------------------------------------

struct FOnWaterBodyIslandChangedParams
{
	FOnWaterBodyIslandChangedParams(const FPropertyChangedEvent& InPropertyChangedEvent = FPropertyChangedEvent(/*InProperty = */nullptr))
		: PropertyChangedEvent(InPropertyChangedEvent)
	{}

	/** Provides some additional context about how the water body island data has changed (property, type of change...) */
	FPropertyChangedEvent PropertyChangedEvent;

	/** Indicates that property related to the water body island's visual shape has changed */
	bool bShapeOrPositionChanged = false;

	/** Indicates that a property affecting the terrain weightmaps has changed */
	bool bWeightmapSettingsChanged = false;
};


// ----------------------------------------------------------------------------------

UCLASS(Blueprintable)
class WATER_API AWaterBodyIsland : public AActor, public IWaterBrushActorInterface
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin IWaterBrushActorInterface interface
	virtual bool AffectsLandscape() const override { return true; }
	virtual bool AffectsWaterMesh() const override { return false; }
	virtual bool CanEverAffectWaterMesh() const override { return false; }

#if WITH_EDITOR
	virtual const FWaterCurveSettings& GetWaterCurveSettings() const { return WaterCurveSettings; }
	virtual const FWaterBodyHeightmapSettings& GetWaterHeightmapSettings() const override { return WaterHeightmapSettings; }
	virtual const TMap<FName, FWaterBodyWeightmapSettings>& GetLayerWeightmapSettings() const override { return WaterWeightmapSettings; }
	virtual ETextureRenderTargetFormat GetBrushRenderTargetFormat() const override;
	virtual void GetBrushRenderDependencies(TSet<UObject*>& OutDependencies) const override;
#endif //WITH_EDITOR
	//~ End IWaterBrushActorInterface interface

	UFUNCTION(BlueprintCallable, Category=Water)
	UWaterSplineComponent* GetWaterSpline() const { return SplineComp; }

	void UpdateHeight();

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	
#if WITH_EDITOR
	virtual void GetActorDescProperties(FPropertyPairsMap& PropertyPairsMap) const override;
	void UpdateOverlappingWaterBodyComponents();
	void UpdateActorIcon();
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	UPROPERTY(Category = Terrain, EditAnywhere, BlueprintReadWrite)
	FWaterCurveSettings WaterCurveSettings;

	UPROPERTY(Category = Terrain, EditAnywhere, BlueprintReadWrite)
	FWaterBodyHeightmapSettings WaterHeightmapSettings;

	UPROPERTY(Category = Terrain, EditAnywhere, BlueprintReadWrite)
	TMap<FName, FWaterBodyWeightmapSettings> WaterWeightmapSettings;

	UPROPERTY(Transient)
	TObjectPtr<UBillboardComponent> ActorIcon;
#endif

protected:
	virtual void Destroyed() override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	UE_DEPRECATED(5.1, "Use the version of this function taking FOnWaterBodyIslandChangedParams in parameter")
	void UpdateAll() {}
	void UpdateAll(const FOnWaterBodyIslandChangedParams& InParams);
	
	void OnWaterSplineDataChanged(const FOnWaterSplineDataChangedParams& InParams);
	void OnWaterBodyIslandChanged(const FOnWaterBodyIslandChangedParams& InParams);

	UE_DEPRECATED(5.1, "Use OnWaterSplineDataChanged")
	void OnSplineDataChanged() {}

	UE_DEPRECATED(5.1, "Use the version of this function taking a FOnWaterBodyIslandChangedParams in parameter")
	void OnWaterBodyIslandChanged(bool bShapeOrPositionChanged, bool bWeightmapSettingsChanged) {}

#endif

protected:
	/**
	 * The spline data attached to this water type.
	 */
	UPROPERTY(VisibleAnywhere, Category = Water, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWaterSplineComponent> SplineComp;
};
