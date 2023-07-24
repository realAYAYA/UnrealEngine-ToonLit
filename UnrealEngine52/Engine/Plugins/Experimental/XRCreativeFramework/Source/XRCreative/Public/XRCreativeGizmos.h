// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseGizmos/CombinedTransformGizmo.h"
#include "XRCreativeGizmos.generated.h"


UCLASS()
class UXRCreativeGizmoBuilder : public UCombinedTransformGizmoBuilder
{
	GENERATED_BODY()

public:
	UXRCreativeGizmoBuilder();
};


UCLASS(Blueprintable)
class AXRCreativeBaseTransformGizmoActor : public ACombinedTransformGizmoActor
{
	GENERATED_BODY()

public:
	AXRCreativeBaseTransformGizmoActor();

protected:
	// Only used during Setup(...) component methods; set before calling those or ConstructDefaults
	float GizmoLineThickness = 3.0f;

	UGizmoArrowComponent* SetupAxisArrow(UGizmoArrowComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis);
	UGizmoRectangleComponent* SetupPlaneRect(UGizmoRectangleComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis0, const FVector& InAxis1);
	UGizmoCircleComponent* SetupAxisRotateCircle(UGizmoCircleComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis);
	UGizmoRectangleComponent* SetupAxisScaleRect(UGizmoRectangleComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis0, const FVector& InAxis1);
	UGizmoRectangleComponent* SetupPlaneScaleFunc(UGizmoRectangleComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis0, const FVector& InAxis1);

	void ConstructDefaults(ETransformGizmoSubElements EnableElements);
};


UCLASS()
class AXRCreativeTRSGizmoActor : public AXRCreativeBaseTransformGizmoActor
{
	GENERATED_BODY()

public:
	AXRCreativeTRSGizmoActor()
	{
		ConstructDefaults(ETransformGizmoSubElements::FullTranslateRotateScale);
	}
};


UCLASS()
class AXRCreativeTRUSGizmoActor : public AXRCreativeBaseTransformGizmoActor
{
	GENERATED_BODY()

public:
	AXRCreativeTRUSGizmoActor()
	{
		ConstructDefaults(ETransformGizmoSubElements::TranslateRotateUniformScale);
	}
};


UCLASS()
class AXRCreativeTRGizmoActor : public AXRCreativeBaseTransformGizmoActor
{
	GENERATED_BODY()

public:
	AXRCreativeTRGizmoActor()
	{
		ConstructDefaults(ETransformGizmoSubElements::StandardTranslateRotate);
	}
};
