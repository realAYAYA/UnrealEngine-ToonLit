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

// need this to implement hover
#include "BaseGizmos/GizmoBaseComponent.h"

#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "ContextObjectStore.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CombinedTransformGizmo)


#define LOCTEXT_NAMESPACE "UCombinedTransformGizmo"


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
	}



	if ((Elements & ETransformGizmoSubElements::ScaleUniform) != ETransformGizmoSubElements::None)
	{
		float BoxSize = 14.0f;
		UGizmoBoxComponent* ScaleComponent = AddDefaultBoxComponent(World, NewActor, GizmoViewContext, FLinearColor::Black,
			FVector(BoxSize/2, BoxSize/2, BoxSize/2), FVector(BoxSize, BoxSize, BoxSize));
		NewActor->UniformScale = ScaleComponent;
	}



	auto MakeAxisScaleFunc = [&](const FLinearColor& Color, const FVector& Axis0, const FVector& Axis1)
	{
		UGizmoRectangleComponent* ScaleComponent = AddDefaultRectangleComponent(World, NewActor, GizmoViewContext, Color, Axis0, Axis1);
		ScaleComponent->OffsetX = 140.0f; ScaleComponent->OffsetY = -10.0f;
		ScaleComponent->LengthX = 7.0f; ScaleComponent->LengthY = 20.0f;
		ScaleComponent->Thickness = GizmoLineThickness;
		ScaleComponent->bOrientYAccordingToCamera = true;
		ScaleComponent->NotifyExternalPropertyUpdates();
		ScaleComponent->SegmentFlags = 0x1 | 0x2 | 0x4; // | 0x8;
		return ScaleComponent;
	};
	if ((Elements & ETransformGizmoSubElements::ScaleAxisX) != ETransformGizmoSubElements::None)
	{
		NewActor->AxisScaleX = MakeAxisScaleFunc(FLinearColor::Red, FVector(1, 0, 0), FVector(0, 0, 1));
	}
	if ((Elements & ETransformGizmoSubElements::ScaleAxisY) != ETransformGizmoSubElements::None)
	{
		NewActor->AxisScaleY = MakeAxisScaleFunc(FLinearColor::Green, FVector(0, 1, 0), FVector(0, 0, 1));
	}
	if ((Elements & ETransformGizmoSubElements::ScaleAxisZ) != ETransformGizmoSubElements::None)
	{
		NewActor->AxisScaleZ = MakeAxisScaleFunc(FLinearColor::Blue, FVector(0, 0, 1), FVector(1, 0, 0));
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
	IsNonUniformScaleAllowed = MoveTemp(IsNonUniformScaleAllowedIn);
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

	bool bShouldShowNonUniformScale = IsNonUniformScaleAllowed();
	for (UPrimitiveComponent* Component : NonuniformScaleComponents)
	{
		Component->SetVisibility(bShouldShowNonUniformScale);
	}

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
		AddAxisTranslationGizmo(GizmoActor->TranslateX, GizmoComponent, AxisXSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->TranslateX);
	}
	if (GizmoActor->TranslateY != nullptr)
	{
		AddAxisTranslationGizmo(GizmoActor->TranslateY, GizmoComponent, AxisYSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->TranslateY);
	}
	if (GizmoActor->TranslateZ != nullptr)
	{
		AddAxisTranslationGizmo(GizmoActor->TranslateZ, GizmoComponent, AxisZSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->TranslateZ);
	}


	if (GizmoActor->TranslateYZ != nullptr)
	{
		AddPlaneTranslationGizmo(GizmoActor->TranslateYZ, GizmoComponent, AxisXSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->TranslateYZ);
	}
	if (GizmoActor->TranslateXZ != nullptr)
	{
		AddPlaneTranslationGizmo(GizmoActor->TranslateXZ, GizmoComponent, AxisYSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->TranslateXZ);
	}
	if (GizmoActor->TranslateXY != nullptr)
	{
		AddPlaneTranslationGizmo(GizmoActor->TranslateXY, GizmoComponent, AxisZSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->TranslateXY);
	}

	if (GizmoActor->RotateX != nullptr)
	{
		AddAxisRotationGizmo(GizmoActor->RotateX, GizmoComponent, AxisXSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->RotateX);
	}
	if (GizmoActor->RotateY != nullptr)
	{
		AddAxisRotationGizmo(GizmoActor->RotateY, GizmoComponent, AxisYSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->RotateY);
	}
	if (GizmoActor->RotateZ != nullptr)
	{
		AddAxisRotationGizmo(GizmoActor->RotateZ, GizmoComponent, AxisZSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->RotateZ);
	}


	// only need these if scaling enabled. Essentially these are just the unit axes, regardless
	// of what 3D axis is in use, we will tell the ParameterSource-to-3D-Scale mapper to
	// use the coordinate axes
	UnitAxisXSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 0, false, this);
	UnitAxisYSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 1, false, this);
	UnitAxisZSource = UGizmoComponentAxisSource::Construct(GizmoComponent, 2, false, this);

	if (GizmoActor->UniformScale != nullptr)
	{
		AddUniformScaleGizmo(GizmoActor->UniformScale, GizmoComponent, CameraAxisSource, CameraAxisSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->UniformScale);
	}

	if (GizmoActor->AxisScaleX != nullptr)
	{
		AddAxisScaleGizmo(GizmoActor->AxisScaleX, GizmoComponent, AxisXSource, UnitAxisXSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->AxisScaleX);
		NonuniformScaleComponents.Add(GizmoActor->AxisScaleX);
	}
	if (GizmoActor->AxisScaleY != nullptr)
	{
		AddAxisScaleGizmo(GizmoActor->AxisScaleY, GizmoComponent, AxisYSource, UnitAxisYSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->AxisScaleY);
		NonuniformScaleComponents.Add(GizmoActor->AxisScaleY);
	}
	if (GizmoActor->AxisScaleZ != nullptr)
	{
		AddAxisScaleGizmo(GizmoActor->AxisScaleZ, GizmoComponent, AxisZSource, UnitAxisZSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->AxisScaleZ);
		NonuniformScaleComponents.Add(GizmoActor->AxisScaleZ);
	}

	if (GizmoActor->PlaneScaleYZ != nullptr)
	{
		AddPlaneScaleGizmo(GizmoActor->PlaneScaleYZ, GizmoComponent, AxisXSource, UnitAxisXSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->PlaneScaleYZ);
		NonuniformScaleComponents.Add(GizmoActor->PlaneScaleYZ);
	}
	if (GizmoActor->PlaneScaleXZ != nullptr)
	{
		UPlanePositionGizmo* Gizmo = (UPlanePositionGizmo *)AddPlaneScaleGizmo(GizmoActor->PlaneScaleXZ, GizmoComponent, AxisYSource, UnitAxisYSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->PlaneScaleXZ);
		NonuniformScaleComponents.Add(GizmoActor->PlaneScaleXZ);
	}
	if (GizmoActor->PlaneScaleXY != nullptr)
	{
		AddPlaneScaleGizmo(GizmoActor->PlaneScaleXY, GizmoComponent, AxisZSource, UnitAxisZSource, TransformSource, StateTarget);
		ActiveComponents.Add(GizmoActor->PlaneScaleXY);
		NonuniformScaleComponents.Add(GizmoActor->PlaneScaleXY);
	}
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

	StateTarget->BeginUpdate();

	USceneComponent* GizmoComponent = GizmoActor->GetRootComponent();
	FTransform GizmoTransform = NewTransform;
	if (bKeepGizmoUnscaled)
	{
		GizmoTransform.SetScale3D(FVector(1, 1, 1));
	}
	GizmoComponent->SetWorldTransform(GizmoTransform);
	ActiveTarget->SetTransform(NewTransform);

	StateTarget->EndUpdate();
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
	GizmoActor->SetActorHiddenInGame(bVisible == false);
#if WITH_EDITOR
	GizmoActor->SetIsTemporarilyHiddenInEditor(bVisible == false);
#endif
}


UInteractiveGizmo* UCombinedTransformGizmo::AddAxisTranslationGizmo(
	UPrimitiveComponent* AxisComponent, USceneComponent* RootComponent,
	IGizmoAxisSource* AxisSource,
	IGizmoTransformSource* TransformSource,
	IGizmoStateTarget* StateTargetIn)
{
	// create axis-position gizmo, axis-position parameter will drive translation
	UAxisPositionGizmo* TranslateGizmo = Cast<UAxisPositionGizmo>(GetGizmoManager()->CreateGizmo(AxisPositionBuilderIdentifier));
	check(TranslateGizmo);

	// axis source provides the translation axis
	TranslateGizmo->AxisSource = Cast<UObject>(AxisSource);

	// parameter source maps axis-parameter-change to translation of TransformSource's transform
	UGizmoAxisTranslationParameterSource* ParamSource = UGizmoAxisTranslationParameterSource::Construct(AxisSource, TransformSource, this);
	ParamSource->PositionConstraintFunction = [this](const FVector& Pos, FVector& Snapped) { return PositionSnapFunction(Pos, Snapped); };
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
	IGizmoStateTarget* StateTargetIn)
{
	// create axis-position gizmo, axis-position parameter will drive translation
	UPlanePositionGizmo* TranslateGizmo = Cast<UPlanePositionGizmo>(GetGizmoManager()->CreateGizmo(PlanePositionBuilderIdentifier));
	check(TranslateGizmo);

	// axis source provides the translation axis
	TranslateGizmo->AxisSource = Cast<UObject>(AxisSource);

	// parameter source maps axis-parameter-change to translation of TransformSource's transform
	UGizmoPlaneTranslationParameterSource* ParamSource = UGizmoPlaneTranslationParameterSource::Construct(AxisSource, TransformSource, this);
	ParamSource->PositionConstraintFunction = [this](const FVector& Pos, FVector& Snapped) { return PositionSnapFunction(Pos, Snapped); };
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
	AngleSource->RotationConstraintFunction = [this](const FQuat& DeltaRotation){ return RotationSnapFunction(DeltaRotation); };
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
	for (UInteractiveGizmo* Gizmo : ActiveGizmos)
	{
		GetGizmoManager()->DestroyGizmo(Gizmo);
	}
	ActiveGizmos.SetNum(0);
	ActiveComponents.SetNum(0);
	NonuniformScaleComponents.SetNum(0);

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

	// only snap if we want snapping obvs
	if (bSnapToWorldGrid == false)
	{
		return false;
	}

	// only snap to world grid when using world axes
	EToolContextCoordinateSystem CoordSystem = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	if (CoordSystem != EToolContextCoordinateSystem::World)
	{
		return false;
	}

	USceneSnappingManager* SnapManager = USceneSnappingManager::Find(GetGizmoManager());
	if (!SnapManager)
	{
		return false;
	}

	FSceneSnapQueryRequest Request;
	Request.RequestType = ESceneSnapQueryType::Position;
	Request.TargetTypes = ESceneSnapQueryTargetType::Grid;
	Request.Position = WorldPosition;
	if ( bGridSizeIsExplicit )
	{
		Request.GridSize = ExplicitGridSize;
	}
	TArray<FSceneSnapQueryResult> Results;
	if (SnapManager->ExecuteSceneSnapQuery(Request, Results))
	{
		SnappedPositionOut = Results[0].Position;
		return true;
	};

	return false;
}

FQuat UCombinedTransformGizmo::RotationSnapFunction(const FQuat& DeltaRotation) const
{
	FQuat SnappedDeltaRotation = DeltaRotation;

	// only snap if we want snapping obvs
	if (!bSnapToWorldGrid)
	{
		return SnappedDeltaRotation;
	}

	// To match our position snapping behavior, only snap when using world axes.
	// Note that if we someday want to snap in local mode, this function will need further
	// changing because the quaternion is given and snapped in world space, whereas we
	// would want to snap it relative to the local frame start orientation.
	EToolContextCoordinateSystem CoordSystem = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	if (CoordSystem != EToolContextCoordinateSystem::World)
	{
		return SnappedDeltaRotation;
	}

	USceneSnappingManager* SnapManager = USceneSnappingManager::Find(GetGizmoManager());
	if (!SnapManager)
	{
		return SnappedDeltaRotation;
	}

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

	return SnappedDeltaRotation;
}

#undef LOCTEXT_NAMESPACE

