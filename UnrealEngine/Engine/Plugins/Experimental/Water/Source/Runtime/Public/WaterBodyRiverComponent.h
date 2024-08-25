// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WaterBodyComponent.h"
#include "WaterBodyRiverComponent.generated.h"

class UMaterialInstanceDynamic;
class USplineMeshComponent;

// ----------------------------------------------------------------------------------

UCLASS(Blueprintable)
class WATER_API UWaterBodyRiverComponent : public UWaterBodyComponent
{
	GENERATED_UCLASS_BODY()
	friend class AWaterBodyRiver;
public:
	/** UWaterBodyComponent Interface */
	virtual EWaterBodyType GetWaterBodyType() const override { return EWaterBodyType::River; }
	virtual TArray<UPrimitiveComponent*> GetCollisionComponents(bool bInOnlyEnabledComponents = true) const override;
	virtual TArray<UPrimitiveComponent*> GetStandardRenderableComponents() const override;
	virtual UMaterialInstanceDynamic* GetRiverToLakeTransitionMaterialInstance() override;
	virtual UMaterialInstanceDynamic* GetRiverToOceanTransitionMaterialInstance() override;
	virtual UMaterialInterface* GetRiverToLakeTransitionMaterial() const override;
	virtual UMaterialInterface* GetRiverToOceanTransitionMaterial() const override;

#if WITH_EDITOR
	virtual TArray<UPrimitiveComponent*> GetBrushRenderableComponents() const override;
#endif //WITH_EDITOR

	UFUNCTION(BlueprintCallable, Category = Rendering)
	void SetLakeTransitionMaterial(UMaterialInterface* InMat);

	UFUNCTION(BlueprintCallable, Category = Rendering)
	void SetOceanTransitionMaterial(UMaterialInterface* InMat);

	UFUNCTION(BlueprintCallable, Category = Rendering)
	void SetLakeAndOceanTransitionMaterials(UMaterialInterface* InLakeTransition, UMaterialInterface* InOceanTransition);

protected:
	/** UWaterBodyComponent Interface */
	virtual void Reset() override;
	virtual void UpdateMaterialInstances() override;
	virtual void OnUpdateBody(bool bWithExclusionVolumes) override;
	virtual bool GenerateWaterBodyMesh(UE::Geometry::FDynamicMesh3& OutMesh, UE::Geometry::FDynamicMesh3* OutDilatedMesh = nullptr) const override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
#if WITH_EDITOR
	virtual void OnPostEditChangeProperty(FOnWaterBodyChangedParams& InOutOnWaterBodyChangedParams) override;

	virtual const TCHAR* GetWaterSpriteTextureName() const override;
#endif

	void CreateOrUpdateLakeTransitionMID();
	void CreateOrUpdateOceanTransitionMID();

	void GenerateMeshes();
	void UpdateSplineMesh(USplineMeshComponent* MeshComp, int32 SplinePointIndex);

protected:
	UPROPERTY(NonPIEDuplicateTransient)
	TArray<TObjectPtr<USplineMeshComponent>> SplineMeshComponents;

	/** Material used when a river is overlapping a lake. */
	UPROPERTY(Category = Rendering, EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "River to Lake Transition"))
	TObjectPtr<UMaterialInterface> LakeTransitionMaterial;

	UPROPERTY(Category = Debug, VisibleInstanceOnly, Transient, NonPIEDuplicateTransient, TextExportTransient, meta = (DisplayAfter = "LakeTransitionMaterial"))
	TObjectPtr<UMaterialInstanceDynamic> LakeTransitionMID;

	/** This is the material used when a river is overlapping the ocean. */
	UPROPERTY(Category = Rendering, EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "River to Ocean Transition"))
	TObjectPtr<UMaterialInterface> OceanTransitionMaterial;

	UPROPERTY(Category = Debug, VisibleInstanceOnly, Transient, NonPIEDuplicateTransient, TextExportTransient, meta = (DisplayAfter = "OceanTransitionMaterial"))
	TObjectPtr<UMaterialInstanceDynamic> OceanTransitionMID;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
