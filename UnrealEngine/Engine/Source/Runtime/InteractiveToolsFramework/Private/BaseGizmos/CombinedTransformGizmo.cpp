// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGizmos/CombinedTransformGizmo.h"
#include "InteractiveGizmoManager.h"
#include "SceneQueries/SceneSnappingManager.h"
#include "BaseGizmos/AxisPositionGizmo.h"
#include "BaseGizmos/PlanePositionGizmo.h"
#include "BaseGizmos/AxisAngleGizmo.h"
#include "BaseGizmos/GizmoComponents.h"

#include "BaseGizmos/GizmoArrowComponent.h"
#include "BaseGizmos/GizmoRectangleComponent.h"
#include "BaseGizmos/GizmoCircleComponent.h"
#include "BaseGizmos/GizmoBoxComponent.h"
#include "BaseGizmos/GizmoLineHandleComponent.h"
#include "BaseGizmos/GizmoViewContext.h"

#include "MathUtil.h"
#include "VectorUtil.h"

// need this to implement hover
#include "BaseGizmos/GizmoBaseComponent.h"

#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "ContextObjectStore.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CombinedTransformGizmo)


#define LOCTEXT_NAMESPACE "UCombinedTransformGizmo"

namespace CombinedTransformGizmoLocals
{
	// Looks at a gizmo actor and figures out what sub element flags must have been active when creating it.
	ETransformGizmoSubElements GetSubElementFlagsFromActor(ACombinedTransformGizmoActor* GizmoActor)
	{
		ETransformGizmoSubElements Elements = ETransformGizmoSubElements::None;
		if (!GizmoActor)
		{
			return Elements;
		}

		if (GizmoActor->TranslateX) { Elements |= ETransformGizmoSubElements::TranslateAxisX; }
		if (GizmoActor->TranslateY) { Elements |= ETransformGizmoSubElements::TranslateAxisY; }
		if (GizmoActor->TranslateZ) { Elements |= ETransformGizmoSubElements::TranslateAxisZ; }
		if (GizmoActor->TranslateXY) { Elements |= ETransformGizmoSubElements::TranslatePlaneXY; }
		if (GizmoActor->TranslateYZ) { Elements |= ETransformGizmoSubElements::TranslatePlaneYZ; }
		if (GizmoActor->TranslateXZ) { Elements |= ETransformGizmoSubElements::TranslatePlaneXZ; }

		if (GizmoActor->RotateX) { Elements |= ETransformGizmoSubElements::RotateAxisX; }
		if (GizmoActor->RotateY) { Elements |= ETransformGizmoSubElements::RotateAxisY; }
		if (GizmoActor->RotateZ) { Elements |= ETransformGizmoSubElements::RotateAxisZ; }

		if (GizmoActor->AxisScaleX) { Elements |= ETransformGizmoSubElements::ScaleAxisX; }
		if (GizmoActor->AxisScaleY) { Elements |= ETransformGizmoSubElements::ScaleAxisY; }
		if (GizmoActor->AxisScaleZ) { Elements |= ETransformGizmoSubElements::ScaleAxisZ; }
		if (GizmoActor->PlaneScaleXY) { Elements |= ETransformGizmoSubElements::ScalePlaneXY; }
		if (GizmoActor->PlaneScaleYZ) { Elements |= ETransformGizmoSubElements::ScalePlaneYZ; }
		if (GizmoActor->PlaneScaleXZ) { Elements |= ETransformGizmoSubElements::ScalePlaneXZ; }

		if (GizmoActor->UniformScale) { Elements |= ETransformGizmoSubElements::ScaleUniform; }

		return Elements;
	}
}

ACombinedTransformGizmoActor::ACombinedTransformGizmoActor()
{
	// root component is a hidden sphere
	USphereComponent* SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("GizmoCenter"));
	RootComponent = SphereComponent;
	SphereComponent->InitSphereRadius(1.0f);
	SphereComponent->SetVisibility(false);
	SphereComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}



ACombinedTransformGizmoActor* ACombinedTransformGizmoActor::ConstructDefault3AxisGizmo(UWorld* World, UGizmoViewContext* GizmoViewContext)
{
	return ConstructCustom3AxisGizmo(World, GizmoViewContext,
		ETransformGizmoSubElements::TranslateAllAxes |
		ETransformGizmoSubElements::TranslateAllPlanes |
		ETransformGizmoSubElements::RotateAllAxes |
		ETransformGizmoSubElements::ScaleAllAxes |
		ETransformGizmoSubElements::ScaleAllPlanes |
		ETransformGizmoSubElements::ScaleUniform
	);
}


ACombinedTransformGizmoActor* ACombinedTransformGizmoActor::ConstructCustom3AxisGizmo(
	UWorld* World, UGizmoViewContext* GizmoViewContext,
	ETransformGizmoSubElements Elements)
{
	FActorSpawnParameters SpawnInfo;
	ACombinedTransformGizmoActor* NewActor = World->SpawnActor<ACombinedTransformGizmoActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);

	float GizmoLineThickness = 3.0f;


	auto MakeAxisArrowFunc = [&](const FLinearColor& Color, const FVector& Axis)
	{
		UGizmoArrowComponent* Component = AddDefaultArrowComponent(World, NewActor, GizmoViewContext, Color, Axis, 60.0f);
		Component->Gap = 20.0f;
		Component->Thickness = GizmoLineThickness;
		Component->NotifyExternalPropertyUpdates();
		return Component;
	};
	if ((Elements & ETransformGizmoSubElements::TranslateAxisX) != ETransformGizmoSubElements::None)
	{
		NewActor->TranslateX = MakeAxisArrowFunc(FLinearColor::Red, FVector(1, 0, 0));
	}
	if ((Elements & ETransformGizmoSubElements::TranslateAxisY) != ETransformGizmoSubElements::None)
	{
		NewActor->TranslateY = MakeAxisArrowFunc(FLinearColor::Green, FVector(0, 1, 0));
	}
	if ((Elements & ETransformGizmoSubElements::TranslateAxisZ) != ETransformGizmoSubElements::None)
	{
		NewActor->TranslateZ = MakeAxisArrowFunc(FLinearColor::Blue, FVector(0, 0, 1));
	}


	auto MakePlaneRectFunc = [&](const FLinearColor& Color, const FVector& Axis0, const FVector& Axis1)
	{
		UGizmoRectangleComponent* Component = AddDefaultRectangleComponent(World, NewActor, GizmoViewContext, Color, Axis0, Axis1);
		Component->LengthX = Component->LengthY = 30.0f;
		Component->SegmentFlags = 0x2 | 0x4;
		Component->Thickness = GizmoLineThickness;
		Component->NotifyExternalPropertyUpdates();
		return Component;
	};
	if ((Elements & ETransformGizmoSubElements::TranslatePlaneYZ) != ETransformGizmoSubElements::None)
	{
		NewActor->TranslateYZ = MakePlaneRectFunc(FLinearColor::Red, FVector(0, 1, 0), FVector(0, 0, 1));
	}
	if ((Elements & ETransformGizmoSubElements::TranslatePlaneXZ) != ETransformGizmoSubElements::None)
	{
		NewActor->TranslateXZ = MakePlaneRectFunc(FLinearColor::Green, FVector(1, 0, 0), FVector(0, 0, 1));
	}
	if ((Elements & ETransformGizmoSubElements::TranslatePlaneXY) != ETransformGizmoSubElements::None)
	{
		NewActor->TranslateXY = MakePlaneRectFunc(FLinearColor::Blue, FVector(1, 0, 0), FVector(0, 1, 0));
	}

	auto MakeAxisRotateCircleFunc = [&](const FLinearColor& Color, const FVector& Axis)
	{
		UGizmoCircleComponent* Component = AddDefaultCircleComponent(World, NewActor, GizmoViewContext, Color, Axis, 120.0f);
		Component->Thickness = GizmoLineThickness;
		Component->NotifyExternalPropertyUpdates();
		return Component;
	};

	bool bAnyRotate = false;
	if ((Elements & ETransformGizmoSubElements::RotateAxisX) != ETransformGizmoSubElements::None)
	{
		NewActor->RotateX = MakeAxisRotateCircleFunc(FLinearColor::Red, FVector(1, 0, 0));
		bAnyRotate = true;
	}
	if ((Elements & ETransformGizmoSubElements::RotateAxisY) != ETransformGizmoSubElements::None)
	{
		NewActor->RotateY = MakeAxisRotateCircleFunc(FLinearColor::Green, FVector(0, 1, 0));
		bAnyRotate = true;
	}
	if ((Elements & ETransformGizmoSubElements::RotateAxisZ) != ETransformGizmoSubElements::None)
	{
		NewActor->RotateZ = MakeAxisRotateCircleFunc(FLinearColor::Blue, FVector(0, 0, 1));
		bAnyRotate = true;
	}


	// add a non-interactive view-aligned circle element, so the axes look like a sphere.
	if (bAnyRotate)
	{
		UGizmoCircleComponent* SphereEdge = NewObject<UGizmoCircleComponent>(NewActor);
		NewActor->AddInstanceComponent(SphereEdge);
		SphereEdge->AttachToComponent(NewActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		SphereEdge->SetGizmoViewContext(GizmoViewContext);
		SphereEdge->Color = FLinearColor::Gray;
		SphereEdge->Thickness = 1.0f;
		SphereEdge->Radius = 120.0f;
		SphereEdge->bViewAligned = true;
		SphereEdge->RegisterComponent();
		NewActor->RotationSphere = SphereEdge;
	}



	if ((Elements & ETransformGizmoSubElements::ScaleUniform) != ETransformGizmoSubElements::None)
	{
		float BoxSize = 14.0f;
		UGizmoBoxComponent* ScaleComponent = AddDefaultBoxComponent(World, NewActor, GizmoViewContext, FLinearColor::Black,
			FVector(BoxSize/2, BoxSize/2, BoxSize/2), FVector(BoxSize, BoxSize, BoxSize));
		NewActor->UniformScale = ScaleComponent;
	}



	auto MakeAxisScaleFunc = [&](const FLinearColor& Color, const FVector& Axis0, const FVector& Axis1, bool bLockSinglePlane)
	{
		UGizmoRectangleComponent* ScaleComponent = AddDefaultRectangleComponent(World, NewActor, GizmoViewContext, Color, Axis0, Axis1);
		ScaleComponent->OffsetX = 140.0f; ScaleComponent->OffsetY = -10.0f;
		ScaleComponent->LengthX = 7.0f; ScaleComponent->LengthY = 20.0f;
		ScaleComponent->Thickness = GizmoLineThickness;
		ScaleComponent->bOrientYAccordingToCamera = !bLockSinglePlane;
		ScaleComponent->NotifyExternalPropertyUpdates();
		ScaleComponent->SegmentFlags = 0x1 | 0x2 | 0x4; // | 0x8;
		return ScaleComponent;
	};

	// This is designed so we can properly handle the visual orientations of the scale handles under the condition of a
	// planar gizmo (such as in the UV Editor).
	// In this case we want to lock the handle on to the other axis of the plane, rather than use the component's camera orientation option. This requires
	// both tracking how many axes are being requested and also *which* axes are requested, in order to configure the correct planar basis vectors.
	// In the case of a single axis, we have to pick a cross axis arbitrarily, but we also keep the auto orientation mode on the component active, so the initial
	// choice isn't as critical. If we want to some day have a single axis handle that is locked, we may need to revisit this again.
	auto ConfigureAdditionalAxis = [&Elements](ETransformGizmoSubElements AxisToTest, int32& TotalAxisCount, FVector& NewPerpendicularAxis) {
		if ((Elements & ETransformGizmoSubElements::ScaleAxisX & AxisToTest) != ETransformGizmoSubElements::None)
		{
			TotalAxisCount++;
			NewPerpendicularAxis = FVector(1, 0, 0);
			return;
		}
		if ((Elements & ETransformGizmoSubElements::ScaleAxisY & AxisToTest) != ETransformGizmoSubElements::None)
		{
			TotalAxisCount++;
			NewPerpendicularAxis = FVector(0, 1, 0);
			return;
		}
		if ((Elements & ETransformGizmoSubElements::ScaleAxisZ & AxisToTest) != ETransformGizmoSubElements::None)
		{
			TotalAxisCount++;
			NewPerpendicularAxis = FVector(0, 0, 1);
			return;
		}
	};

	if ((Elements & ETransformGizmoSubElements::ScaleAxisX) != ETransformGizmoSubElements::None)
	{
		int32 TotalAxisCount = 1;
		FVector PerpendicularAxis(0,1,0);
		ConfigureAdditionalAxis(ETransformGizmoSubElements::ScaleAxisY, TotalAxisCount, PerpendicularAxis);
		ConfigureAdditionalAxis(ETransformGizmoSubElements::ScaleAxisZ, TotalAxisCount, PerpendicularAxis);
		NewActor->AxisScaleX = MakeAxisScaleFunc(FLinearColor::Red, FVector(1, 0, 0), PerpendicularAxis, TotalAxisCount == 2);
	}

	if ((Elements & ETransformGizmoSubElements::ScaleAxisY) != ETransformGizmoSubElements::None)
	{
		int32 TotalAxisCount = 1;
		FVector PerpendicularAxis(1, 0, 0);
		ConfigureAdditionalAxis(ETransformGizmoSubElements::ScaleAxisX, TotalAxisCount, PerpendicularAxis);
		ConfigureAdditionalAxis(ETransformGizmoSubElements::ScaleAxisZ, TotalAxisCount, PerpendicularAxis);
		NewActor->AxisScaleY = MakeAxisScaleFunc(FLinearColor::Green, FVector(0, 1, 0), PerpendicularAxis, TotalAxisCount == 2);
	}

	if ((Elements & ETransformGizmoSubElements::ScaleAxisZ) != ETransformGizmoSubElements::None)
	{
		int32 TotalAxisCount = 1;
		FVector PerpendicularAxis(1, 0, 0);
		ConfigureAdditionalAxis(ETransformGizmoSubElements::ScaleAxisY, TotalAxisCount, PerpendicularAxis);
		ConfigureAdditionalAxis(ETransformGizmoSubElements::ScaleAxisX, TotalAxisCount, PerpendicularAxis);
		NewActor->AxisScaleZ = MakeAxisScaleFunc(FLinearColor::Blue, FVector(0, 0, 1), PerpendicularAxis, TotalAxisCount == 2);
	}


	auto MakePlaneScaleFunc = [&](const FLinearColor& Color, const FVector& Axis0, const FVector& Axis1)
	{
		UGizmoRectangleComponent* ScaleComponent = AddDefaultRectangleComponent(World, NewActor, GizmoViewContext, Color, Axis0, Axis1);
		ScaleComponent->OffsetX = ScaleComponent->OffsetY = 120.0f;
		ScaleComponent->LengthX = ScaleComponent->LengthY = 20.0f;
		ScaleComponent->Thickness = GizmoLineThickness;
		ScaleComponent->NotifyExternalPropertyUpdates();
		ScaleComponent->SegmentFlags = 0x2 | 0x4;
		return ScaleComponent;
	};
	if ((Elements & ETransformGizmoSubElements::ScalePlaneYZ) != ETransformGizmoSubElements::None)
	{
		NewActor->PlaneScaleYZ = MakePlaneScaleFunc(FLinearColor::Red, FVector(0, 1, 0), FVector(0, 0, 1));
	}
	if ((Elements & ETransformGizmoSubElements::ScalePlaneXZ) != ETransformGizmoSubElements::None)
	{
		NewActor->PlaneScaleXZ = MakePlaneScaleFunc(FLinearColor::Green, FVector(1, 0, 0), FVector(0, 0, 1));
	}
	if ((Elements & ETransformGizmoSubElements::ScalePlaneXY) != ETransformGizmoSubElements::None)
	{
		NewActor->PlaneScaleXY = MakePlaneScaleFunc(FLinearColor::Blue, FVector(1, 0, 0), FVector(0, 1, 0));
	}


	return NewActor;
}




ACombinedTransformGizmoActor* FCombinedTransformGizmoActorFactory::CreateNewGizmoActor(UWorld* World) const
{
	return ACombinedTransformGizmoActor::ConstructCustom3AxisGizmo(World, GizmoViewContext, EnableElements);
}



UInteractiveGizmo* UCombinedTransformGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	UCombinedTransformGizmo* NewGizmo = NewObject<UCombinedTransformGizmo>(SceneState.GizmoManager);
	NewGizmo->SetWorld(SceneState.World);

	UGizmoViewContext* GizmoViewContext = SceneState.ToolManager->GetContextObjectStore()->FindContext<UGizmoViewContext>();
	check(GizmoViewContext && GizmoViewContext->IsValidLowLevel());

	// use default gizmo actor if client has not given us a new builder
	NewGizmo->SetGizmoActorBuilder(GizmoActorBuilder ? GizmoActorBuilder : MakeShared<FCombinedTransformGizmoActorFactory>(GizmoViewContext));

	NewGizmo->SetSubGizmoBuilderIdentifiers(AxisPositionBuilderIdentifier, PlanePositionBuilderIdentifier, AxisAngleBuilderIdentifier);

	// override default hover function if proposed
	if (UpdateHoverFunction)
	{
		NewGizmo->SetUpdateHoverFunction(UpdateHoverFunction);
	}

	if (UpdateCoordSystemFunction)
	{
		NewGizmo->SetUpdateCoordSystemFunction(UpdateCoordSystemFunction);
	}

	return NewGizmo;
}



void UCombinedTransformGizmo::SetWorld(UWorld* WorldIn)
{
	this->World = WorldIn;
}

void UCombinedTransformGizmo::SetGizmoActorBuilder(TSharedPtr<FCombinedTransformGizmoActorFactory> Builder)
{
	GizmoActorBuilder = Builder;
}

void UCombinedTransformGizmo::SetSubGizmoBuilderIdentifiers(FString AxisPositionBuilderIdentifierIn, FString PlanePositionBuilderIdentifierIn, FString AxisAngleBuilderIdentifierIn)
{
	AxisPositionBuilderIdentifier = AxisPositionBuilderIdentifierIn;
	PlanePositionBuilderIdentifier = PlanePositionBuilderIdentifierIn;
	AxisAngleBuilderIdentifier = AxisAngleBuilderIdentifierIn;
}

void UCombinedTransformGizmo::SetUpdateHoverFunction(TFunction<void(UPrimitiveComponent*, bool)> HoverFunction)
{
	UpdateHoverFunction = HoverFunction;
}

void UCombinedTransformGizmo::SetUpdateCoordSystemFunction(TFunction<void(UPrimitiveComponent*, EToolContextCoordinateSystem)> CoordSysFunction)
{
	UpdateCoordSystemFunction = CoordSysFunction;
}

void UCombinedTransformGizmo::SetWorldAlignmentFunctions(
	TUniqueFunction<bool()>&& ShouldAlignTranslationIn,
	TUniqueFunction<bool(const FRay&, FVector&)>&& TranslationAlignmentRayCasterIn)
{
	// Save these so that later changes of gizmo target keep the settings.
	ShouldAlignDestination = MoveTemp(ShouldAlignTranslationIn);
	DestinationAlignmentRayCaster = MoveTemp(TranslationAlignmentRayCasterIn);

	// We allow this function to be called after Setup(), so modify any existing translation/rotation sub gizmos.
	// Unfortunately we keep all the sub gizmos in one list, and the scaling gizmos are differentiated from the
	// translation ones mainly in the components they use. So this ends up being a slightly messy set of checks,
	// but it didn't seem worth keeping a segregated list for something that will only happen once.
	for (UInteractiveGizmo* SubGizmo : this->ActiveGizmos)
	{
		if (UAxisPositionGizmo* CastGizmo = Cast<UAxisPositionGizmo>(SubGizmo))
		{
			if (UGizmoComponentHitTarget* CastHitTarget = Cast<UGizmoComponentHitTarget>(CastGizmo->HitTarget.GetObject()))
			{
				if (CastHitTarget->Component == GizmoActor->TranslateX
					|| CastHitTarget->Component == GizmoActor->TranslateY
					|| CastHitTarget->Component == GizmoActor->TranslateZ)
				{
					CastGizmo->ShouldUseCustomDestinationFunc = [this]() { return ShouldAlignDestination(); };
					CastGizmo->CustomDestinationFunc = 
						[this](const UAxisPositionGizmo::FCustomDestinationParams& Params, FVector& OutputPoint) { 
						return DestinationAlignmentRayCaster(*Params.WorldRay, OutputPoint); 
					};
				}
			}
		}
		if (UPlanePositionGizmo* CastGizmo = Cast<UPlanePositionGizmo>(SubGizmo))
		{
			if (UGizmoComponentHitTarget* CastHitTarget = Cast<UGizmoComponentHitTarget>(CastGizmo->HitTarget.GetObject()))
			{
				if (CastHitTarget->Component == GizmoActor->TranslateXY
					|| CastHitTarget->Component == GizmoActor->TranslateXZ
					|| CastHitTarget->Component == GizmoActor->TranslateYZ)
				{
					CastGizmo->ShouldUseCustomDestinationFunc = [this]() { return ShouldAlignDestination(); };
					CastGizmo->CustomDestinationFunc =
						[this](const UPlanePositionGizmo::FCustomDestinationParams& Params, FVector& OutputPoint) {
						return DestinationAlignmentRayCaster(*Params.WorldRay, OutputPoint);
					};
				}
			}
		}
		if (UAxisAngleGizmo* CastGizmo = Cast<UAxisAngleGizmo>(SubGizmo))
		{
			CastGizmo->ShouldUseCustomDestinationFunc = [this]() { return ShouldAlignDestination(); };
			CastGizmo->CustomDestinationFunc =
				[this](const UAxisAngleGizmo::FCustomDestinationParams& Params, FVector& OutputPoint) {
				return DestinationAlignmentRayCaster(*Params.WorldRay, OutputPoint);
			};
		}
	}
}

void UCombinedTransformGizmo::SetDisallowNegativeScaling(bool bDisallow)
{
	if (bDisallowNegativeScaling != bDisallow)
	{
		bDisallowNegativeScaling = bDisallow;
		for (UInteractiveGizmo* SubGizmo : this->ActiveGizmos)
		{
			if (UAxisPositionGizmo* CastGizmo = Cast<UAxisPositionGizmo>(SubGizmo))
			{
				if (UGizmoAxisScaleParameterSource* ParamSource = Cast<UGizmoAxisScaleParameterSource>(CastGizmo->ParameterSource.GetObject()))
				{
					ParamSource->bClampToZero = bDisallow;
				}
			}
			if (UPlanePositionGizmo* CastGizmo = Cast<UPlanePositionGizmo>(SubGizmo))
			{
				if (UGizmoPlaneScaleParameterSource* ParamSource = Cast<UGizmoPlaneScaleParameterSource>(CastGizmo->ParameterSource.GetObject()))
				{
					ParamSource->bClampToZero = bDisallow;
				}
			}
		}
	}
}

void UCombinedTransformGizmo::SetIsNonUniformScaleAllowedFunction(TUniqueFunction<bool()>&& IsNonUniformScaleAllowedIn)
{
	IsNonUniformScaleAllowedFunc = MoveTemp(IsNonUniformScaleAllowedIn);
}


void UCombinedTransformGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	UpdateHoverFunction = [](UPrimitiveComponent* Component, bool bHovering)
	{
		if (Cast<UGizmoBaseComponent>(Component) != nullptr)
		{
			Cast<UGizmoBaseComponent>(Component)->UpdateHoverState(bHovering);
		}
	};

	UpdateCoordSystemFunction = [](UPrimitiveComponent* Component, EToolContextCoordinateSystem CoordSystem)
	{
		if (Cast<UGizmoBaseComponent>(Component) != nullptr)
		{
			Cast<UGizmoBaseComponent>(Component)->UpdateWorldLocalState(CoordSystem == EToolContextCoordinateSystem::World);
		}
	};

	GizmoActor = GizmoActorBuilder->CreateNewGizmoActor(World);
}



void UCombinedTransformGizmo::Shutdown()
{
	ClearActiveTarget();

	if (GizmoActor)
	{
		GizmoActor->Destroy();
		GizmoActor = nullptr;
	}
}



void UCombinedTransformGizmo::UpdateCameraAxisSource()
{
	FViewCameraState CameraState;
	GetGizmoManager()->GetContextQueriesAPI()->GetCurrentViewState(CameraState);
	if (CameraAxisSource != nullptr && GizmoActor != nullptr)
	{
		CameraAxisSource->Origin = GizmoActor->GetTransform().GetLocation();
		CameraAxisSource->Direction = -CameraState.Forward();
		CameraAxisSource->TangentX = CameraState.Right();
		CameraAxisSource->TangentY = CameraState.Up();
	}
}


void UCombinedTransformGizmo::Tick(float DeltaTime)
{	
	if (bUseContextCoordinateSystem)
	{
		CurrentCoordinateSystem = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	}
	check(CurrentCoordinateSystem == EToolContextCoordinateSystem::World || CurrentCoordinateSystem == EToolContextCoordinateSystem::Local)
	
	FToolContextSnappingConfiguration SnappingConfig = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentSnappingSettings();
	RelativeTranslationSnapping.UpdateContextValue(SnappingConfig.bEnableAbsoluteWorldSnapping == false);

	bool bUseLocalAxes = (CurrentCoordinateSystem == EToolContextCoordinateSystem::Local);
	if (AxisXSource != nullptr && AxisYSource != nullptr && AxisZSource != nullptr)
	{
		AxisXSource->bLocalAxes = bUseLocalAxes;
		AxisYSource->bLocalAxes = bUseLocalAxes;
		AxisZSource->bLocalAxes = bUseLocalAxes;
	}
	if (UpdateCoordSystemFunction)
	{
		for (UPrimitiveComponent* Component : ActiveComponents)
		{
			UpdateCoordSystemFunction(Component, CurrentCoordinateSystem);
		}
	}

	// apply dynamic visibility filtering to sub-gizmos

	auto SetSubGizmoTypeVisibility = [this](TArray<FSubGizmoInfo>& GizmoInfos, bool bVisible)
	{
		for (FSubGizmoInfo& GizmoInfo : GizmoInfos)
		{
			if (GizmoInfo.Component.IsValid())
			{
				GizmoInfo.Component->SetVisibility(bVisible);
			}
		}
	};

	if (bUseContextGizmoMode)
	{
		ActiveGizmoMode = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentTransformGizmoMode();
	}
	EToolContextTransformGizmoMode UseGizmoMode = ActiveGizmoMode;

	bool bShouldShowTranslation =
		(UseGizmoMode == EToolContextTransformGizmoMode::Combined || UseGizmoMode == EToolContextTransformGizmoMode::Translation);
	bool bShouldShowRotation =
		(UseGizmoMode == EToolContextTransformGizmoMode::Combined || UseGizmoMode == EToolContextTransformGizmoMode::Rotation);
	bool bShouldShowUniformScale =
		(UseGizmoMode == EToolContextTransformGizmoMode::Combined || UseGizmoMode == EToolContextTransformGizmoMode::Scale);
	bool bShouldShowNonUniformScale = 
		(UseGizmoMode == EToolContextTransformGizmoMode::Combined || UseGizmoMode == EToolContextTransformGizmoMode::Scale)
		&& IsNonUniformScaleAllowedFunc();

	SetSubGizmoTypeVisibility(TranslationSubGizmos, bShouldShowTranslation);
	SetSubGizmoTypeVisibility(RotationSubGizmos, bShouldShowRotation);
	SetSubGizmoTypeVisibility(UniformScaleSubGizmos, bShouldShowUniformScale);
	SetSubGizmoTypeVisibility(NonUniformScaleSubGizmos, bShouldShowNonUniformScale);

	UpdateCameraAxisSource();
}



void UCombinedTransformGizmo::SetActiveTarget(UTransformProxy* Target, IToolContextTransactionProvider* TransactionProvider)
{
	if (ActiveTarget != nullptr)
	{
		ClearActiveTarget();
	}

	ActiveTarget = Target;

	// move gizmo to target location
	USceneComponent* GizmoComponent = GizmoActor->GetRootComponent();

	FTransform TargetTransform = Target->GetTransform();
	FTransform GizmoTransform = TargetTransform;
	GizmoTransform.SetScale3D(FVector(1, 1, 1));
	GizmoComponent->SetWorldTransform(GizmoTransform);

	UGizmoScaledAndUnscaledTransformSources* TransformSource = UGizmoScaledAndUnscaledTransformSources::Construct(
		UGizmoTransformProxyTransformSource::Construct(ActiveTarget, this), 
		GizmoComponent, this);

	// This state target emits an explicit FChange that moves the GizmoActor root component during undo/redo.
	// It also opens/closes the Transaction that saves/restores the target object locations.
	if (TransactionProvider == nullptr)
	{
		TransactionProvider = GetGizmoManager();
	}
	StateTarget = UGizmoTransformChangeStateTarget::Construct(GizmoComponent,
		LOCTEXT("UCombinedTransformGizmoTransaction", "Transform"), TransactionProvider, this);
	StateTarget->DependentChangeSources.Add(MakeUnique<FTransformProxyChangeSource>(Target));

	CameraAxisSource = NewObject<UGizmoConstantFrameAxisSource>(this);

	// root component provides local X/Y/Z axis, identified by AxisIndex
	AxisXSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 0, true, this);
	AxisYSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 1, true, this);
	AxisZSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 2, true, this);

	// todo should we hold onto these?
	if (GizmoActor->TranslateX != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisTranslationGizmo(GizmoActor->TranslateX, GizmoComponent, AxisXSource, TransformSource, StateTarget, 0);
		ActiveComponents.Add(GizmoActor->TranslateX);
		TranslationSubGizmos.Add( FSubGizmoInfo{ GizmoActor->TranslateX, NewGizmo } );
	}
	if (GizmoActor->TranslateY != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisTranslationGizmo(GizmoActor->TranslateY, GizmoComponent, AxisYSource, TransformSource, StateTarget, 1);
		ActiveComponents.Add(GizmoActor->TranslateY);
		TranslationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->TranslateY, NewGizmo });
	}
	if (GizmoActor->TranslateZ != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisTranslationGizmo(GizmoActor->TranslateZ, GizmoComponent, AxisZSource, TransformSource, StateTarget, 2);
		ActiveComponents.Add(GizmoActor->TranslateZ);
		TranslationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->TranslateZ, NewGizmo });
	}


	if (GizmoActor->TranslateYZ != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddPlaneTranslationGizmo(GizmoActor->TranslateYZ, GizmoComponent, AxisXSource, TransformSource, StateTarget, 1, 2);
		ActiveComponents.Add(GizmoActor->TranslateYZ);
		TranslationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->TranslateYZ, NewGizmo });
	}
	if (GizmoActor->TranslateXZ != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddPlaneTranslationGizmo(GizmoActor->TranslateXZ, GizmoComponent, AxisYSource, TransformSource, StateTarget, 2, 0);	// flip here corresponds to UGizmoComponentAxisSource::GetTangentVectors()
		ActiveComponents.Add(GizmoActor->TranslateXZ);
		TranslationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->TranslateXZ, NewGizmo });
	}
	if (GizmoActor->TranslateXY != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddPlaneTranslationGizmo(GizmoActor->TranslateXY, GizmoComponent, AxisZSource, TransformSource, StateTarget, 0, 1);
		ActiveComponents.Add(GizmoActor->TranslateXY);
		TranslationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->TranslateXY, NewGizmo });
	}

	if (GizmoActor->RotateX != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisRotationGizmo(GizmoActor->RotateX, GizmoComponent, AxisXSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->RotateX);
		RotationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->RotateX, NewGizmo });
	}
	if (GizmoActor->RotateY != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisRotationGizmo(GizmoActor->RotateY, GizmoComponent, AxisYSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->RotateY);
		RotationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->RotateY, NewGizmo });
	}
	if (GizmoActor->RotateZ != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisRotationGizmo(GizmoActor->RotateZ, GizmoComponent, AxisZSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->RotateZ);
		RotationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->RotateZ, NewGizmo });
	}
	if (GizmoActor->RotationSphere != nullptr)
	{
		// no gizmo for the sphere currently
		ActiveComponents.Add(GizmoActor->RotationSphere);
		RotationSubGizmos.Add(FSubGizmoInfo{ GizmoActor->RotationSphere, nullptr });
	}


	// only need these if scaling enabled. Essentially these are just the unit axes, regardless
	// of what 3D axis is in use, we will tell the ParameterSource-to-3D-Scale mapper to
	// use the coordinate axes
	UnitAxisXSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 0, false, this);
	UnitAxisYSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 1, false, this);
	UnitAxisZSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 2, false, this);

	if (GizmoActor->UniformScale != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddUniformScaleGizmo(GizmoActor->UniformScale, GizmoComponent, CameraAxisSource, CameraAxisSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->UniformScale);
		UniformScaleSubGizmos.Add(FSubGizmoInfo{ GizmoActor->UniformScale, NewGizmo });
	}

	if (GizmoActor->AxisScaleX != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisScaleGizmo(GizmoActor->AxisScaleX, GizmoComponent, AxisXSource, UnitAxisXSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->AxisScaleX);
		NonUniformScaleSubGizmos.Add(FSubGizmoInfo{ GizmoActor->AxisScaleX, NewGizmo });
	}
	if (GizmoActor->AxisScaleY != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisScaleGizmo(GizmoActor->AxisScaleY, GizmoComponent, AxisYSource, UnitAxisYSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->AxisScaleY);
		NonUniformScaleSubGizmos.Add(FSubGizmoInfo{ GizmoActor->AxisScaleY, NewGizmo });
	}
	if (GizmoActor->AxisScaleZ != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddAxisScaleGizmo(GizmoActor->AxisScaleZ, GizmoComponent, AxisZSource, UnitAxisZSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->AxisScaleZ);
		NonUniformScaleSubGizmos.Add(FSubGizmoInfo{ GizmoActor->AxisScaleZ, NewGizmo });
	}

	if (GizmoActor->PlaneScaleYZ != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddPlaneScaleGizmo(GizmoActor->PlaneScaleYZ, GizmoComponent, AxisXSource, UnitAxisXSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->PlaneScaleYZ);
		NonUniformScaleSubGizmos.Add(FSubGizmoInfo{ GizmoActor->PlaneScaleYZ, NewGizmo });
	}
	if (GizmoActor->PlaneScaleXZ != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddPlaneScaleGizmo(GizmoActor->PlaneScaleXZ, GizmoComponent, AxisYSource, UnitAxisYSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->PlaneScaleXZ);
		NonUniformScaleSubGizmos.Add(FSubGizmoInfo{ GizmoActor->PlaneScaleXZ, NewGizmo });
	}
	if (GizmoActor->PlaneScaleXY != nullptr)
	{
		UInteractiveGizmo* NewGizmo = AddPlaneScaleGizmo(GizmoActor->PlaneScaleXY, GizmoComponent, AxisZSource, UnitAxisZSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->PlaneScaleXY);
		NonUniformScaleSubGizmos.Add(FSubGizmoInfo{ GizmoActor->PlaneScaleXY, NewGizmo });
	}

	OnSetActiveTarget.Broadcast(this, ActiveTarget);
}

FTransform UCombinedTransformGizmo::GetGizmoTransform() const
{
	USceneComponent* GizmoComponent = GizmoActor->GetRootComponent();
	return GizmoComponent->GetComponentTransform();
}

void UCombinedTransformGizmo::ReinitializeGizmoTransform(const FTransform& NewTransform, bool bKeepGizmoUnscaled)
{
	// To update the gizmo location without triggering any callbacks, we temporarily
	// store a copy of the callback list, detach them, reposition, and then reattach
	// the callbacks.
	USceneComponent* GizmoComponent = GizmoActor->GetRootComponent();
	auto temp = GizmoComponent->TransformUpdated;
	GizmoComponent->TransformUpdated.Clear();
	FTransform GizmoTransform = NewTransform;
	if (bKeepGizmoUnscaled)
	{
		GizmoTransform.SetScale3D(FVector(1, 1, 1));
	}
	GizmoComponent->SetWorldTransform(GizmoTransform);
	GizmoComponent->TransformUpdated = temp;

	// The underlying proxy has an existing way to reinitialize its transform without callbacks.
	bool bSavedSetPivotMode = ActiveTarget->bSetPivotMode;
	ActiveTarget->bSetPivotMode = true;
	ActiveTarget->SetTransform(NewTransform);
	ActiveTarget->bSetPivotMode = bSavedSetPivotMode;
}

void UCombinedTransformGizmo::SetNewGizmoTransform(const FTransform& NewTransform, bool bKeepGizmoUnscaled)
{
	check(ActiveTarget != nullptr);

	BeginTransformEditSequence();
	UpdateTransformDuringEditSequence(NewTransform, bKeepGizmoUnscaled);
	EndTransformEditSequence();
}

void UCombinedTransformGizmo::BeginTransformEditSequence()
{
	if (ensure(StateTarget))
	{
		StateTarget->BeginUpdate();
	}
}

void UCombinedTransformGizmo::EndTransformEditSequence()
{
	if (ensure(StateTarget))
	{
		StateTarget->EndUpdate();
	}
}

void UCombinedTransformGizmo::UpdateTransformDuringEditSequence(const FTransform& NewTransform, bool bKeepGizmoUnscaled)
{
	check(ActiveTarget != nullptr);

	USceneComponent* GizmoComponent = GizmoActor->GetRootComponent();
	FTransform GizmoTransform = NewTransform;
	if (bKeepGizmoUnscaled)
	{
		GizmoTransform.SetScale3D(FVector(1, 1, 1));
	}
	GizmoComponent->SetWorldTransform(GizmoTransform);
	ActiveTarget->SetTransform(NewTransform);
}

void UCombinedTransformGizmo::SetNewChildScale(const FVector& NewChildScale)
{
	FTransform NewTransform = ActiveTarget->GetTransform();
	NewTransform.SetScale3D(NewChildScale);

	bool bSavedSetPivotMode = ActiveTarget->bSetPivotMode;
	ActiveTarget->bSetPivotMode = true;
	ActiveTarget->SetTransform(NewTransform);
	ActiveTarget->bSetPivotMode = bSavedSetPivotMode;
}


void UCombinedTransformGizmo::SetVisibility(bool bVisible)
{
	bool bPreviousVisibility = !GizmoActor->IsHidden();

	GizmoActor->SetActorHiddenInGame(bVisible == false);
#if WITH_EDITOR
	GizmoActor->SetIsTemporarilyHiddenInEditor(bVisible == false);
#endif

	if (bPreviousVisibility != bVisible)
	{
		OnVisibilityChanged.Broadcast(this, bVisible);
	}
}

void UCombinedTransformGizmo::SetDisplaySpaceTransform(TOptional<FTransform> TransformIn)
{
	if (DisplaySpaceTransform.IsSet() != TransformIn.IsSet()
		|| (TransformIn.IsSet() && !TransformIn.GetValue().Equals(DisplaySpaceTransform.GetValue())))
	{
		DisplaySpaceTransform = TransformIn;
		OnDisplaySpaceTransformChanged.Broadcast(this, TransformIn);
	}
}

ETransformGizmoSubElements UCombinedTransformGizmo::GetGizmoElements()
{
	using namespace CombinedTransformGizmoLocals;

	return GetSubElementFlagsFromActor(GizmoActor);
}

UInteractiveGizmo* UCombinedTransformGizmo::AddAxisTranslationGizmo(
	UPrimitiveComponent* AxisComponent, USceneComponent* RootComponent,
	IGizmoAxisSource* AxisSource,
	IGizmoTransformSource* TransformSource,
	IGizmoStateTarget* StateTargetIn,
	int AxisIndex)
{
	// create axis-position gizmo, axis-position parameter will drive translation
	UAxisPositionGizmo* TranslateGizmo = Cast<UAxisPositionGizmo>(GetGizmoManager()->CreateGizmo(AxisPositionBuilderIdentifier));
	check(TranslateGizmo);

	// axis source provides the translation axis
	TranslateGizmo->AxisSource = Cast<UObject>(AxisSource);

	// parameter source maps axis-parameter-change to translation of TransformSource's transform
	UGizmoAxisTranslationParameterSource* ParamSource = UGizmoAxisTranslationParameterSource::Construct(AxisSource, TransformSource, this);
	ParamSource->PositionConstraintFunction = [this](const FVector& Pos, FVector& Snapped) { return PositionSnapFunction(Pos, Snapped); };
	ParamSource->AxisDeltaConstraintFunction = [this, AxisIndex](double AxisDelta, double& SnappedAxisDelta) { return PositionAxisDeltaSnapFunction(AxisDelta, SnappedAxisDelta, AxisIndex); };
	TranslateGizmo->ParameterSource = ParamSource;

	// sub-component provides hit target
	UGizmoComponentHitTarget* HitTarget = UGizmoComponentHitTarget::Construct(AxisComponent, this);
	if (this->UpdateHoverFunction)
	{
		HitTarget->UpdateHoverFunction = [AxisComponent, this](bool bHovering) { this->UpdateHoverFunction(AxisComponent, bHovering); };
	}
	TranslateGizmo->HitTarget = HitTarget;

	TranslateGizmo->StateTarget = Cast<UObject>(StateTargetIn);

	TranslateGizmo->ShouldUseCustomDestinationFunc = [this]() { return ShouldAlignDestination(); };
	TranslateGizmo->CustomDestinationFunc =
		[this](const UAxisPositionGizmo::FCustomDestinationParams& Params, FVector& OutputPoint) {
		return DestinationAlignmentRayCaster(*Params.WorldRay, OutputPoint);
	};

	ActiveGizmos.Add(TranslateGizmo);
	return TranslateGizmo;
}



UInteractiveGizmo* UCombinedTransformGizmo::AddPlaneTranslationGizmo(
	UPrimitiveComponent* AxisComponent, USceneComponent* RootComponent,
	IGizmoAxisSource* AxisSource,
	IGizmoTransformSource* TransformSource,
	IGizmoStateTarget* StateTargetIn,
	int XAxisIndex, int YAxisIndex)
{
	// create axis-position gizmo, axis-position parameter will drive translation
	UPlanePositionGizmo* TranslateGizmo = Cast<UPlanePositionGizmo>(GetGizmoManager()->CreateGizmo(PlanePositionBuilderIdentifier));
	check(TranslateGizmo);

	// axis source provides the translation axis
	TranslateGizmo->AxisSource = Cast<UObject>(AxisSource);

	// parameter source maps axis-parameter-change to translation of TransformSource's transform
	UGizmoPlaneTranslationParameterSource* ParamSource = UGizmoPlaneTranslationParameterSource::Construct(AxisSource, TransformSource, this);
	ParamSource->PositionConstraintFunction = [this](const FVector& Pos, FVector& Snapped) { return PositionSnapFunction(Pos, Snapped); };
	ParamSource->AxisXDeltaConstraintFunction = [this, XAxisIndex](double AxisDelta, double& SnappedAxisDelta) { return PositionAxisDeltaSnapFunction(AxisDelta, SnappedAxisDelta, XAxisIndex); };
	ParamSource->AxisYDeltaConstraintFunction = [this, YAxisIndex](double AxisDelta, double& SnappedAxisDelta) { return PositionAxisDeltaSnapFunction(AxisDelta, SnappedAxisDelta, YAxisIndex); };
	TranslateGizmo->ParameterSource = ParamSource;

	// sub-component provides hit target
	UGizmoComponentHitTarget* HitTarget = UGizmoComponentHitTarget::Construct(AxisComponent, this);
	if (this->UpdateHoverFunction)
	{
		HitTarget->UpdateHoverFunction = [AxisComponent, this](bool bHovering) { this->UpdateHoverFunction(AxisComponent, bHovering); };
	}
	TranslateGizmo->HitTarget = HitTarget;

	TranslateGizmo->StateTarget = Cast<UObject>(StateTargetIn);

	TranslateGizmo->ShouldUseCustomDestinationFunc = [this]() { return ShouldAlignDestination(); };
	TranslateGizmo->CustomDestinationFunc =
		[this](const UPlanePositionGizmo::FCustomDestinationParams& Params, FVector& OutputPoint) {
		return DestinationAlignmentRayCaster(*Params.WorldRay, OutputPoint);
	};

	ActiveGizmos.Add(TranslateGizmo);
	return TranslateGizmo;
}





UInteractiveGizmo* UCombinedTransformGizmo::AddAxisRotationGizmo(
	UPrimitiveComponent* AxisComponent, USceneComponent* RootComponent,
	IGizmoAxisSource* AxisSource,
	IGizmoTransformSource* TransformSource,
	IGizmoStateTarget* StateTargetIn)
{
	// create axis-angle gizmo, angle will drive axis-rotation
	UAxisAngleGizmo* RotateGizmo = Cast<UAxisAngleGizmo>(GetGizmoManager()->CreateGizmo(AxisAngleBuilderIdentifier));
	check(RotateGizmo);

	// axis source provides the rotation axis
	RotateGizmo->AxisSource = Cast<UObject>(AxisSource);

	// parameter source maps angle-parameter-change to rotation of TransformSource's transform
	UGizmoAxisRotationParameterSource* AngleSource = UGizmoAxisRotationParameterSource::Construct(AxisSource, TransformSource, this);
	// axis rotation is currently only relative so it should only ever snap angle-deltas
	//AngleSource->RotationConstraintFunction = [this](const FQuat& DeltaRotation){ return RotationSnapFunction(DeltaRotation); };
	AngleSource->AngleDeltaConstraintFunction = [this](double AngleDelta, double& SnappedDelta){ return RotationAxisAngleSnapFunction(AngleDelta, SnappedDelta, 0); };
	RotateGizmo->AngleSource = AngleSource;

	// sub-component provides hit target
	UGizmoComponentHitTarget* HitTarget = UGizmoComponentHitTarget::Construct(AxisComponent, this);
	if (this->UpdateHoverFunction)
	{
		HitTarget->UpdateHoverFunction = [AxisComponent, this](bool bHovering) { this->UpdateHoverFunction(AxisComponent, bHovering); };
	}
	RotateGizmo->HitTarget = HitTarget;

	RotateGizmo->StateTarget = Cast<UObject>(StateTargetIn);

	RotateGizmo->ShouldUseCustomDestinationFunc = [this]() { return ShouldAlignDestination(); };
	RotateGizmo->CustomDestinationFunc =
		[this](const UAxisAngleGizmo::FCustomDestinationParams& Params, FVector& OutputPoint) {
		return DestinationAlignmentRayCaster(*Params.WorldRay, OutputPoint);
	};

	ActiveGizmos.Add(RotateGizmo);

	return RotateGizmo;
}



UInteractiveGizmo* UCombinedTransformGizmo::AddAxisScaleGizmo(
	UPrimitiveComponent* AxisComponent, USceneComponent* RootComponent,
	IGizmoAxisSource* GizmoAxisSource, IGizmoAxisSource* ParameterAxisSource,
	IGizmoTransformSource* TransformSource,
	IGizmoStateTarget* StateTargetIn)
{
	// create axis-position gizmo, axis-position parameter will drive scale
	UAxisPositionGizmo* ScaleGizmo = Cast<UAxisPositionGizmo>(GetGizmoManager()->CreateGizmo(AxisPositionBuilderIdentifier));
	ScaleGizmo->bEnableSignedAxis = true;
	check(ScaleGizmo);

	// axis source provides the translation axis
	ScaleGizmo->AxisSource = Cast<UObject>(GizmoAxisSource);

	// parameter source maps axis-parameter-change to translation of TransformSource's transform
	UGizmoAxisScaleParameterSource* ParamSource = UGizmoAxisScaleParameterSource::Construct(ParameterAxisSource, TransformSource, this);
	ParamSource->bClampToZero = bDisallowNegativeScaling;
	ScaleGizmo->ParameterSource = ParamSource;

	// sub-component provides hit target
	UGizmoComponentHitTarget* HitTarget = UGizmoComponentHitTarget::Construct(AxisComponent, this);
	if (this->UpdateHoverFunction)
	{
		HitTarget->UpdateHoverFunction = [AxisComponent, this](bool bHovering) { this->UpdateHoverFunction(AxisComponent, bHovering); };
	}
	ScaleGizmo->HitTarget = HitTarget;

	ScaleGizmo->StateTarget = Cast<UObject>(StateTargetIn);

	ActiveGizmos.Add(ScaleGizmo);
	return ScaleGizmo;
}



UInteractiveGizmo* UCombinedTransformGizmo::AddPlaneScaleGizmo(
	UPrimitiveComponent* AxisComponent, USceneComponent* RootComponent,
	IGizmoAxisSource* GizmoAxisSource, IGizmoAxisSource* ParameterAxisSource,
	IGizmoTransformSource* TransformSource,
	IGizmoStateTarget* StateTargetIn)
{
	// create axis-position gizmo, axis-position parameter will drive scale
	UPlanePositionGizmo* ScaleGizmo = Cast<UPlanePositionGizmo>(GetGizmoManager()->CreateGizmo(PlanePositionBuilderIdentifier));
	ScaleGizmo->bEnableSignedAxis = true;
	check(ScaleGizmo);

	// axis source provides the translation axis
	ScaleGizmo->AxisSource = Cast<UObject>(GizmoAxisSource);

	// parameter source maps axis-parameter-change to translation of TransformSource's transform
	UGizmoPlaneScaleParameterSource* ParamSource = UGizmoPlaneScaleParameterSource::Construct(ParameterAxisSource, TransformSource, this);
	ParamSource->bClampToZero = bDisallowNegativeScaling;
	ParamSource->bUseEqualScaling = true;
	ScaleGizmo->ParameterSource = ParamSource;

	// sub-component provides hit target
	UGizmoComponentHitTarget* HitTarget = UGizmoComponentHitTarget::Construct(AxisComponent, this);
	if (this->UpdateHoverFunction)
	{
		HitTarget->UpdateHoverFunction = [AxisComponent, this](bool bHovering) { this->UpdateHoverFunction(AxisComponent, bHovering); };
	}
	ScaleGizmo->HitTarget = HitTarget;

	ScaleGizmo->StateTarget = Cast<UObject>(StateTargetIn);

	ActiveGizmos.Add(ScaleGizmo);
	return ScaleGizmo;
}





UInteractiveGizmo* UCombinedTransformGizmo::AddUniformScaleGizmo(
	UPrimitiveComponent* ScaleComponent, USceneComponent* RootComponent,
	IGizmoAxisSource* GizmoAxisSource, IGizmoAxisSource* ParameterAxisSource,
	IGizmoTransformSource* TransformSource,
	IGizmoStateTarget* StateTargetIn)
{
	// create plane-position gizmo, plane-position parameter will drive scale
	UPlanePositionGizmo* ScaleGizmo = Cast<UPlanePositionGizmo>(GetGizmoManager()->CreateGizmo(PlanePositionBuilderIdentifier));
	check(ScaleGizmo);

	// axis source provides the translation plane
	ScaleGizmo->AxisSource = Cast<UObject>(GizmoAxisSource);

	// parameter source maps axis-parameter-change to translation of TransformSource's transform
	UGizmoUniformScaleParameterSource* ParamSource = UGizmoUniformScaleParameterSource::Construct(ParameterAxisSource, TransformSource, this);
	//ParamSource->PositionConstraintFunction = [this](const FVector& Pos, FVector& Snapped) { return PositionSnapFunction(Pos, Snapped); };
	ScaleGizmo->ParameterSource = ParamSource;

	// sub-component provides hit target
	UGizmoComponentHitTarget* HitTarget = UGizmoComponentHitTarget::Construct(ScaleComponent, this);
	if (this->UpdateHoverFunction)
	{
		HitTarget->UpdateHoverFunction = [ScaleComponent, this](bool bHovering) { this->UpdateHoverFunction(ScaleComponent, bHovering); };
	}
	ScaleGizmo->HitTarget = HitTarget;

	ScaleGizmo->StateTarget = Cast<UObject>(StateTargetIn);

	ActiveGizmos.Add(ScaleGizmo);
	return ScaleGizmo;
}



void UCombinedTransformGizmo::ClearActiveTarget()
{
	OnAboutToClearActiveTarget.Broadcast(this, ActiveTarget);

	for (UInteractiveGizmo* Gizmo : ActiveGizmos)
	{
		GetGizmoManager()->DestroyGizmo(Gizmo);
	}
	ActiveGizmos.SetNum(0);
	ActiveComponents.SetNum(0);
	TranslationSubGizmos.SetNum(0);
	RotationSubGizmos.SetNum(0);
	UniformScaleSubGizmos.SetNum(0);
	NonUniformScaleSubGizmos.SetNum(0);

	CameraAxisSource = nullptr;
	AxisXSource = nullptr;
	AxisYSource = nullptr;
	AxisZSource = nullptr;
	UnitAxisXSource = nullptr;
	UnitAxisYSource = nullptr;
	UnitAxisZSource = nullptr;
	StateTarget = nullptr;

	ActiveTarget = nullptr;
}




bool UCombinedTransformGizmo::PositionSnapFunction(const FVector& WorldPosition, FVector& SnappedPositionOut) const
{
	SnappedPositionOut = WorldPosition;

	// only snap world positions if we want world position snapping...
	if (bSnapToWorldGrid == false || RelativeTranslationSnapping.IsEnabled() == true)
	{
		return false;
	}

	// we can only snap positions in world coordinate system
	EToolContextCoordinateSystem CoordSystem = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	if (CoordSystem != EToolContextCoordinateSystem::World)
	{
		return false;
	}

	// need a snapping manager
	if ( USceneSnappingManager* SnapManager = USceneSnappingManager::Find(GetGizmoManager()) )
	{
		FSceneSnapQueryRequest Request;
		Request.RequestType = ESceneSnapQueryType::Position;
		Request.TargetTypes = ESceneSnapQueryTargetType::Grid;
		if ( bGridSizeIsExplicit )
		{
			Request.GridSize = ExplicitGridSize;
		}
		TArray<FSceneSnapQueryResult> Results;
		Results.Reserve(1);

		Request.Position = WorldPosition;
		if (SnapManager->ExecuteSceneSnapQuery(Request, Results))
		{
			SnappedPositionOut = Results[0].Position;
			return true;
		};
	}

	return false;
}


bool UCombinedTransformGizmo::PositionAxisDeltaSnapFunction(double AxisDelta, double& SnappedDeltaOut, int AxisIndex) const
{
	if (!bSnapToWorldGrid) return false;

	EToolContextCoordinateSystem CoordSystem = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	bool bUseRelativeSnapping = RelativeTranslationSnapping.IsEnabled() || (CoordSystem != EToolContextCoordinateSystem::World);
	if (!bUseRelativeSnapping)
	{
		return false;
	}

	if ( USceneSnappingManager* SnapManager = USceneSnappingManager::Find(GetGizmoManager()) )
	{
		FSceneSnapQueryRequest Request;
		Request.RequestType = ESceneSnapQueryType::Position;
		Request.TargetTypes = ESceneSnapQueryTargetType::Grid;
		if ( bGridSizeIsExplicit )
		{
			Request.GridSize = ExplicitGridSize;
		}
		TArray<FSceneSnapQueryResult> Results;
		Results.Reserve(1);
		
		// this is a bit of a hack, since the snap query only snaps world points, and the grid may not be
		// uniform. A point on the specified X/Y/Z at the delta-distance is snapped, this is ideally
		// equivalent to actually computing a snap of the axis-delta
		Request.Position = FVector::Zero();
		Request.Position[AxisIndex] = AxisDelta;
		if (SnapManager->ExecuteSceneSnapQuery(Request, Results))
		{
			SnappedDeltaOut = Results[0].Position[AxisIndex];
			return true;
		};
	}
	return false;
}




FQuat UCombinedTransformGizmo::RotationSnapFunction(const FQuat& DeltaRotation) const
{
	// note: this is currently unused. Although we can snap to the "rotation grid", since the
	// gizmo only supports axis rotations, it doesn't make sense. Leaving in for now in case
	// a "tumble" handle is added, in which case it makes sense to snap to the world rotation grid...

	FQuat SnappedDeltaRotation = DeltaRotation;

	// only snap world positions if we want world position snapping...
	if (bSnapToWorldRotGrid == false )
	{
		return SnappedDeltaRotation;
	}

	// can only snap absolute rotations in World coordinates
	EToolContextCoordinateSystem CoordSystem = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	if (CoordSystem != EToolContextCoordinateSystem::World)
	{
		return SnappedDeltaRotation;
	}

	// need a snapping manager
	if ( USceneSnappingManager* SnapManager = USceneSnappingManager::Find(GetGizmoManager()) )
	{
		FSceneSnapQueryRequest Request;
		Request.RequestType   = ESceneSnapQueryType::Rotation;
		Request.TargetTypes   = ESceneSnapQueryTargetType::Grid;
		Request.DeltaRotation = DeltaRotation;
		if ( bRotationGridSizeIsExplicit )
		{
			Request.RotGridSize = ExplicitRotationGridSize;
		}
		TArray<FSceneSnapQueryResult> Results;
		if (SnapManager->ExecuteSceneSnapQuery(Request, Results))
		{
			SnappedDeltaRotation = Results[0].DeltaRotation;
		};
	}

	return SnappedDeltaRotation;
}





bool UCombinedTransformGizmo::RotationAxisAngleSnapFunction(double AxisAngleDelta, double& SnappedAxisAngleDeltaOut, int AxisIndex) const
{
	if (!bSnapToWorldRotGrid) return false;

	FToolContextSnappingConfiguration SnappingConfig = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentSnappingSettings();
	if ( SnappingConfig.bEnableRotationGridSnapping )
	{
		double SnapDelta = SnappingConfig.RotationGridAngles.Yaw;		// could use AxisIndex here?
		if ( bRotationGridSizeIsExplicit )
		{
			SnapDelta = ExplicitRotationGridSize.Yaw;
		}
		AxisAngleDelta *= FMathd::RadToDeg;
		SnappedAxisAngleDeltaOut = UE::Geometry::SnapToIncrement(AxisAngleDelta, SnapDelta);
		SnappedAxisAngleDeltaOut *= FMathd::DegToRad;
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

