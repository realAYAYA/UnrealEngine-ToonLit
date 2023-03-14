// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WaterBodyComponent.h"
#include "WaterBodyLakeComponent.generated.h"

class UBoxComponent;
class ULakeCollisionComponent;
class UDEPRECATED_LakeGenerator;

// ----------------------------------------------------------------------------------

UCLASS(Blueprintable)
class WATER_API UWaterBodyLakeComponent : public UWaterBodyComponent
{
	GENERATED_UCLASS_BODY()

	friend class AWaterBodyLake;
public:
	/** UWaterBodyComponent Interface */
	virtual EWaterBodyType GetWaterBodyType() const override { return EWaterBodyType::Lake; }
	virtual TArray<UPrimitiveComponent*> GetCollisionComponents(bool bInOnlyEnabledComponents = true) const override;
	virtual TArray<UPrimitiveComponent*> GetStandardRenderableComponents() const override;
	virtual bool GenerateWaterBodyMesh(UE::Geometry::FDynamicMesh3& OutMesh, UE::Geometry::FDynamicMesh3* OutDilatedMesh = nullptr) const override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

protected:
	/** UWaterBodyComponent Interface */
	virtual void Reset() override;
	virtual void OnUpdateBody(bool bWithExclusionVolumes) override;

#if WITH_EDITOR
	virtual const TCHAR* GetWaterSpriteTextureName() const override;

	virtual FVector GetWaterSpriteLocation() const override;
#endif // WITH_EDITOR

	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<UStaticMeshComponent> LakeMeshComp;

	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<ULakeCollisionComponent> LakeCollision;
};