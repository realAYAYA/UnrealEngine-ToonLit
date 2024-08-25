// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRCreativeGizmos.h"
#include "BaseGizmos/GizmoArrowComponent.h"
#include "BaseGizmos/GizmoBoxComponent.h"
#include "BaseGizmos/GizmoCircleComponent.h"
#include "BaseGizmos/GizmoRectangleComponent.h"
#include "BaseGizmos/GizmoViewContext.h"


UXRCreativeGizmoBuilder::UXRCreativeGizmoBuilder()
{
	AxisPositionBuilderIdentifier = UInteractiveGizmoManager::DefaultAxisPositionBuilderIdentifier;
	PlanePositionBuilderIdentifier = UInteractiveGizmoManager::DefaultPlanePositionBuilderIdentifier;
	AxisAngleBuilderIdentifier = UInteractiveGizmoManager::DefaultAxisAngleBuilderIdentifier;
}


//////////////////////////////////////////////////////////////////////////


AXRCreativeBaseTransformGizmoActor::AXRCreativeBaseTransformGizmoActor()
{
}


UGizmoArrowComponent* AXRCreativeBaseTransformGizmoActor::SetupAxisArrow(UGizmoArrowComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis)
{
	InComponent->SetupAttachment(GetRootComponent());
	InComponent->Direction = InAxis;
	InComponent->Color = InColor;
	InComponent->Length = 60.0f;
	InComponent->Gap = 20.0f;
	InComponent->Thickness = GizmoLineThickness;
	InComponent->NotifyExternalPropertyUpdates();
	InComponent->bHiddenInSceneCapture=true;
	return InComponent;
}


UGizmoRectangleComponent* AXRCreativeBaseTransformGizmoActor::SetupPlaneRect(UGizmoRectangleComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis0, const FVector& InAxis1)
{
	InComponent->SetupAttachment(GetRootComponent());
	InComponent->DirectionX = InAxis0;
	InComponent->DirectionY = InAxis1;
	InComponent->Color = InColor;
	InComponent->LengthX = InComponent->LengthY = 30.0f;
	InComponent->SegmentFlags = 0x2 | 0x4;
	InComponent->Thickness = GizmoLineThickness;
	InComponent->NotifyExternalPropertyUpdates();
	InComponent->bHiddenInSceneCapture=true;
	return InComponent;
}


UGizmoCircleComponent* AXRCreativeBaseTransformGizmoActor::SetupAxisRotateCircle(UGizmoCircleComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis)
{
	InComponent->SetupAttachment(GetRootComponent());
	InComponent->Normal = InAxis;
	InComponent->Color = InColor;
	InComponent->Radius = 100.0f;
	InComponent->Thickness = GizmoLineThickness;
	InComponent->NotifyExternalPropertyUpdates();
	InComponent->bHiddenInSceneCapture=true;
	return InComponent;
}


UGizmoRectangleComponent* AXRCreativeBaseTransformGizmoActor::SetupAxisScaleRect(UGizmoRectangleComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis0, const FVector& InAxis1)
{
	InComponent->SetupAttachment(GetRootComponent());
	InComponent->DirectionX = InAxis0;
	InComponent->DirectionY = InAxis1;
	InComponent->Color = InColor;
	InComponent->OffsetX = 140.0f;
	InComponent->OffsetY = -10.0f;
	InComponent->LengthX = 7.0f;
	InComponent->LengthY = 20.0f;
	InComponent->Thickness = GizmoLineThickness;
	InComponent->bOrientYAccordingToCamera = true;
	InComponent->SegmentFlags = 0x1 | 0x2 | 0x4; // | 0x8;
	InComponent->NotifyExternalPropertyUpdates();
	InComponent->bHiddenInSceneCapture=true;
	return InComponent;
}


UGizmoRectangleComponent* AXRCreativeBaseTransformGizmoActor::SetupPlaneScaleFunc(UGizmoRectangleComponent* InComponent, const FLinearColor& InColor, const FVector& InAxis0, const FVector& InAxis1)
{
	InComponent->SetupAttachment(GetRootComponent());
	InComponent->DirectionX = InAxis0;
	InComponent->DirectionY = InAxis1;
	InComponent->Color = InColor;
	InComponent->OffsetX = InComponent->OffsetY = 120.0f;
	InComponent->LengthX = InComponent->LengthY = 20.0f;
	InComponent->Thickness = GizmoLineThickness;
	InComponent->SegmentFlags = 0x2 | 0x4;
	InComponent->NotifyExternalPropertyUpdates();
	InComponent->bHiddenInSceneCapture=true;
	return InComponent;
}


void AXRCreativeBaseTransformGizmoActor::ConstructDefaults(ETransformGizmoSubElements EnableElements)
{
	if ((EnableElements & ETransformGizmoSubElements::TranslateAxisX) != ETransformGizmoSubElements::None)
	{
		TranslateX = SetupAxisArrow(CreateDefaultSubobject<UGizmoArrowComponent>("TranslateX"), FLinearColor::Red, FVector(1, 0, 0));
	}
	if ((EnableElements & ETransformGizmoSubElements::TranslateAxisY) != ETransformGizmoSubElements::None)
	{
		TranslateY = SetupAxisArrow(CreateDefaultSubobject<UGizmoArrowComponent>("TranslateY"), FLinearColor::Green, FVector(0, 1, 0));
	}
	if ((EnableElements & ETransformGizmoSubElements::TranslateAxisZ) != ETransformGizmoSubElements::None)
	{
		TranslateZ = SetupAxisArrow(CreateDefaultSubobject<UGizmoArrowComponent>("TranslateZ"), FLinearColor::Blue, FVector(0, 0, 1));
	}

	if ((EnableElements & ETransformGizmoSubElements::TranslatePlaneYZ) != ETransformGizmoSubElements::None)
	{
		TranslateYZ = SetupPlaneRect(CreateDefaultSubobject<UGizmoRectangleComponent>("TranslateYZ"), FLinearColor::Red, FVector(0, 1, 0), FVector(0, 0, 1));
	}
	if ((EnableElements & ETransformGizmoSubElements::TranslatePlaneXZ) != ETransformGizmoSubElements::None)
	{
		TranslateXZ = SetupPlaneRect(CreateDefaultSubobject<UGizmoRectangleComponent>("TranslateXZ"), FLinearColor::Green, FVector(1, 0, 0), FVector(0, 0, 1));
	}
	if ((EnableElements & ETransformGizmoSubElements::TranslatePlaneXY) != ETransformGizmoSubElements::None)
	{
		TranslateXY = SetupPlaneRect(CreateDefaultSubobject<UGizmoRectangleComponent>("TranslateXY"), FLinearColor::Blue, FVector(1, 0, 0), FVector(0, 1, 0));
	}

	bool bAnyRotate = false;
	if ((EnableElements & ETransformGizmoSubElements::RotateAxisX) != ETransformGizmoSubElements::None)
	{
		RotateX = SetupAxisRotateCircle(CreateDefaultSubobject<UGizmoCircleComponent>("RotateX"), FLinearColor::Red, FVector(1, 0, 0));
		bAnyRotate = true;
	}
	if ((EnableElements & ETransformGizmoSubElements::RotateAxisY) != ETransformGizmoSubElements::None)
	{
		RotateY = SetupAxisRotateCircle(CreateDefaultSubobject<UGizmoCircleComponent>("RotateY"), FLinearColor::Green, FVector(0, 1, 0));
		bAnyRotate = true;
	}
	if ((EnableElements & ETransformGizmoSubElements::RotateAxisZ) != ETransformGizmoSubElements::None)
	{
		RotateZ = SetupAxisRotateCircle(CreateDefaultSubobject<UGizmoCircleComponent>("RotateZ"), FLinearColor::Blue, FVector(0, 0, 1));
		bAnyRotate = true;
	}

	// add a non-interactive view-aligned circle element, so the axes look like a sphere.
	if (bAnyRotate)
	{
		UGizmoCircleComponent* RotationSphereComp = CreateDefaultSubobject<UGizmoCircleComponent>("RotationSphere");
		RotationSphereComp->SetupAttachment(GetRootComponent());
		RotationSphereComp->Color = FLinearColor::Gray;
		RotationSphereComp->Thickness = GizmoLineThickness * 0.5f;
		RotationSphereComp->Radius = 120.0f;
		RotationSphereComp->bViewAligned = true;
		RotationSphereComp->NotifyExternalPropertyUpdates();
		RotationSphereComp->bHiddenInSceneCapture=true;
		RotationSphere = RotationSphereComp;
	}

	if ((EnableElements & ETransformGizmoSubElements::ScaleUniform) != ETransformGizmoSubElements::None)
	{
		float BoxSize = 14.0f;
		UGizmoBoxComponent* UniformScaleComp = CreateDefaultSubobject<UGizmoBoxComponent>("UniformScale");
		UniformScaleComp->SetupAttachment(GetRootComponent());
		UniformScaleComp->Color = FLinearColor::Black;
		UniformScaleComp->Origin = FVector(BoxSize/2, BoxSize/2, BoxSize/2);
		UniformScaleComp->Dimensions = FVector(BoxSize, BoxSize, BoxSize);
		UniformScaleComp->NotifyExternalPropertyUpdates();
		UniformScaleComp->bHiddenInSceneCapture=true;
		UniformScale = UniformScaleComp;
	}

	if ((EnableElements & ETransformGizmoSubElements::ScaleAxisX) != ETransformGizmoSubElements::None)
	{
		AxisScaleX = SetupAxisScaleRect(CreateDefaultSubobject<UGizmoRectangleComponent>("AxisScaleX"), FLinearColor::Red, FVector(1, 0, 0), FVector(0, 0, 1));
	}
	if ((EnableElements & ETransformGizmoSubElements::ScaleAxisY) != ETransformGizmoSubElements::None)
	{
		AxisScaleY = SetupAxisScaleRect(CreateDefaultSubobject<UGizmoRectangleComponent>("AxisScaleY"), FLinearColor::Green, FVector(0, 1, 0), FVector(0, 0, 1));
	}
	if ((EnableElements & ETransformGizmoSubElements::ScaleAxisZ) != ETransformGizmoSubElements::None)
	{
		AxisScaleZ = SetupAxisScaleRect(CreateDefaultSubobject<UGizmoRectangleComponent>("AxisScaleZ"), FLinearColor::Blue, FVector(0, 0, 1), FVector(1, 0, 0));
	}

	if ((EnableElements & ETransformGizmoSubElements::ScalePlaneYZ) != ETransformGizmoSubElements::None)
	{
		PlaneScaleYZ = SetupPlaneScaleFunc(CreateDefaultSubobject<UGizmoRectangleComponent>("PlaneScaleYZ"), FLinearColor::Red, FVector(0, 1, 0), FVector(0, 0, 1));
	}
	if ((EnableElements & ETransformGizmoSubElements::ScalePlaneXZ) != ETransformGizmoSubElements::None)
	{
		PlaneScaleXZ = SetupPlaneScaleFunc(CreateDefaultSubobject<UGizmoRectangleComponent>("PlaneScaleXZ"), FLinearColor::Green, FVector(1, 0, 0), FVector(0, 0, 1));
	}
	if ((EnableElements & ETransformGizmoSubElements::ScalePlaneXY) != ETransformGizmoSubElements::None)
	{
		PlaneScaleXY = SetupPlaneScaleFunc(CreateDefaultSubobject<UGizmoRectangleComponent>("PlaneScaleXY"), FLinearColor::Blue, FVector(1, 0, 0), FVector(0, 1, 0));
	}
}
