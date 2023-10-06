// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "GizmoArrowComponent.generated.h"

/**
 * Simple Component intended to be used as part of 3D Gizmos.
 * Currently draws the "arrow" as a single 3D line.
 */
UCLASS(ClassGroup = Utility, HideCategories = (Physics, Collision, Lighting, Rendering, Mobile), MinimalAPI)
class UGizmoArrowComponent : public UGizmoBaseComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Options)
	FVector Direction = FVector(0,0,1);

	UPROPERTY(EditAnywhere, Category = Options)
	float Gap = 5.0f;

	UPROPERTY(EditAnywhere, Category = Options)
	float Length = 60.0f;

	UPROPERTY(EditAnywhere, Category = Options)
	float Thickness = 2.0f;

private:
	//~ Begin UPrimitiveComponent Interface.
	INTERACTIVETOOLSFRAMEWORK_API virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	INTERACTIVETOOLSFRAMEWORK_API virtual bool LineTraceComponent(FHitResult& OutHit, const FVector Start, const FVector End, const FCollisionQueryParams& Params) override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
	INTERACTIVETOOLSFRAMEWORK_API virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.
};
