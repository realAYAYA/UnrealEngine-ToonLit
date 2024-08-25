// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WaterBodyComponent.h"
#include "WaterBodyOceanComponent.generated.h"

class UOceanCollisionComponent;
class UOceanBoxCollisionComponent;

// ----------------------------------------------------------------------------------

UCLASS(Blueprintable)
class WATER_API UWaterBodyOceanComponent : public UWaterBodyComponent
{
	GENERATED_UCLASS_BODY()

	friend class AWaterBodyOcean;
public:
	/** UWaterBodyComponent Interface */
	virtual EWaterBodyType GetWaterBodyType() const override { return EWaterBodyType::Ocean; }
	virtual TArray<UPrimitiveComponent*> GetCollisionComponents(bool bInOnlyEnabledComponents = true) const override;
	virtual FVector GetCollisionExtents() const override { return CollisionExtents; }
	virtual void SetHeightOffset(float InHeightOffset) override;
	virtual float GetHeightOffset() const override { return HeightOffset; }
	
#if WITH_EDITOR
	void SetCollisionExtents(const FVector& NewExtents);

	void SetOceanExtent(const FVector2D& NewExtents);

	/** Rebuilds the ocean mesh to completely fill the zone to which it belongs. */
	UFUNCTION(CallInEditor, Category = Water)
	void FillWaterZoneWithOcean();
#endif // WITH_EDITOR

	UE_DEPRECATED(5.1, "Oceans no longer rely on the visual extent parameter making this obsolete. Instead they will be guaranteed to fill the entire water zone to which they belong.")
	void SetVisualExtents(FVector2D) {}
	UE_DEPRECATED(5.1, "Oceans no longer rely on the visual extent parameter making this obsolete. Instead they will be guaranteed to fill the entire water zone to which they belong.")
	FVector2D GetVisualExtents() const { return FVector2D(); }
protected:
	/** UWaterBodyComponent Interface */
	virtual bool IsBodyDynamic() const override { return true; }
	virtual void BeginUpdateWaterBody() override;
	virtual void OnUpdateBody(bool bWithExclusionVolumes) override;
	virtual void Reset() override;
	virtual bool GenerateWaterBodyMesh(UE::Geometry::FDynamicMesh3& OutMesh, UE::Geometry::FDynamicMesh3* OutDilatedMesh = nullptr) const override;

	virtual void PostLoad() override;
	virtual void OnPostRegisterAllComponents() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const;

	virtual void OnPostActorCreated() override;

#if WITH_EDITOR
	virtual void OnPostEditChangeProperty(FOnWaterBodyChangedParams& InOutOnWaterBodyChangedParams) override;

	virtual const TCHAR* GetWaterSpriteTextureName() const override;

	virtual TArray<TSharedRef<FTokenizedMessage>> CheckWaterBodyStatus();

	virtual void OnWaterBodyRenderDataUpdated() override;
#endif
protected:
	UPROPERTY(NonPIEDuplicateTransient)
	TArray<TObjectPtr<UOceanBoxCollisionComponent>> CollisionBoxes;

	UPROPERTY(NonPIEDuplicateTransient)
	TArray<TObjectPtr<UOceanCollisionComponent>> CollisionHullSets;

	UPROPERTY(Category = Collision, EditAnywhere, BlueprintReadOnly)
	FVector CollisionExtents;

	/** The extent of the ocean, centered around water zone to which the ocean belongs. */
	UPROPERTY(Category = Water, EditAnywhere, BlueprintReadOnly)
	FVector2D OceanExtents;

	/** Saved water zone location so that the ocean mesh can be regenerated relative to it and match it perfectly without being loaded. */
	UPROPERTY()
	FVector2D SavedZoneLocation;

	/** If enabled, oceans will always center their mesh/bounds on the owning water zone by using a saved location that is updated whenever the ocean mesh is rebuilt. */
	UPROPERTY(EditAnywhere, Category = Water, AdvancedDisplay)
	bool bCenterOnWaterZone = true;

	UPROPERTY(Transient)
	float HeightOffset = 0.0f;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	FVector2D VisualExtents_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
