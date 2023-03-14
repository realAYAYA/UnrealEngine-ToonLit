// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BaseGizmos/GizmoBaseComponent.h"
#include "GizmoLineHandleComponent.generated.h"


/**
 * Simple Component intended to be used as part of 3D Gizmos.
 * Draws line terminated by a short perpendicular handle line based on parameters.
 */
UCLASS(ClassGroup = Utility, HideCategories = (Physics, Collision, Lighting, Rendering, Mobile))
class INTERACTIVETOOLSFRAMEWORK_API UGizmoLineHandleComponent : public UGizmoBaseComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Options)
	FVector Normal = FVector(0, 0, 1);

	UPROPERTY(EditAnywhere, Category = Options)
	float HandleSize = 10.0f;

	UPROPERTY(EditAnywhere, Category = Options)
	float Thickness = 2.0f;

	UPROPERTY(EditAnywhere, Category = Options)
	FVector Direction = FVector(0, 0, 1.);

	UPROPERTY(EditAnywhere, Category = Options)
	float Length;

	UPROPERTY(EditAnywhere, Category = Options)
	bool bImageScale = true;
private:
	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool LineTraceComponent(FHitResult& OutHit, const FVector Start, const FVector End, const FCollisionQueryParams& Params) override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.
};