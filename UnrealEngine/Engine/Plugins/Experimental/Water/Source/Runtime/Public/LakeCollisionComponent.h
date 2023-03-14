// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ShapeComponent.h"
#include "LakeCollisionComponent.generated.h"

UCLASS(ClassGroup = (Custom))
class WATER_API ULakeCollisionComponent : public UPrimitiveComponent
{
	friend class FLakeCollisionSceneProxy;

	GENERATED_UCLASS_BODY()
public:
	void UpdateCollision(FVector InBoxExtent, bool bSplinePointsChanged);
	
	virtual bool IsZeroExtent() const override { return BoxExtent.IsZero(); }
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const;
	virtual UBodySetup* GetBodySetup() override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// The scene proxy is only for debug purposes :
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	/** Collects custom navigable geometry of component.
    *   Substract the MaxWaveHeight to the Lake collision so nav mesh geometry is exported a ground level
	*	@return true if regular navigable geometry exporting should be run as well */
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;

protected:
	void UpdateBodySetup();
	void CreateLakeBodySetupIfNeeded();
private:
	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<class UBodySetup> CachedBodySetup;

	UPROPERTY()
	FVector BoxExtent;
};