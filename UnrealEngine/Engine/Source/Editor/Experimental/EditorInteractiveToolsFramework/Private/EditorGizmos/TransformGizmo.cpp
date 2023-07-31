// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/TransformGizmo.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseGizmos/AxisSources.h"
#include "BaseGizmos/GizmoElementGroup.h"
#include "BaseGizmos/GizmoElementShapes.h"
#include "BaseGizmos/GizmoMath.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/ParameterSourcesFloat.h"
#include "BaseGizmos/StateTargets.h"
#include "Elements/Interfaces/TypedElementObjectInterface.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EditorModeTools.h"
#include "UnrealEdGlobals.h"
#include "UnrealEngine.h"

#define LOCTEXT_NAMESPACE "UTransformGizmo"

DEFINE_LOG_CATEGORY_STATIC(LogTransformGizmo, Log, All);

void UTransformGizmo::SetDisallowNegativeScaling(bool bDisallow)
{
	bDisallowNegativeScaling = bDisallow;
}

void UTransformGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	SetupBehaviors();
	SetupMaterials();
	SetupOnClickFunctions();

	// @todo: Gizmo element construction will be moved to the UEditorTransformGizmoBuilder to decouple
	// the rendered elements from the transform gizmo.
	GizmoElementRoot = NewObject<UGizmoElementGroup>();
	GizmoElementRoot->SetConstantScale(true);
	GizmoElementRoot->SetHoverMaterial(CurrentAxisMaterial);
	GizmoElementRoot->SetInteractMaterial(CurrentAxisMaterial);
	GizmoElementRoot->SetHoverLineColor(CurrentColor);
	GizmoElementRoot->SetInteractLineColor(CurrentColor);

	bInInteraction = false;
}

void UTransformGizmo::SetupBehaviors()
{
	// Add default mouse hover behavior
	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	HoverBehavior->Initialize(this);
	HoverBehavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY));
	AddInputBehavior(HoverBehavior);

	// Add default mouse input behavior
	MouseBehavior = NewObject<UClickDragInputBehavior>();
	MouseBehavior->Initialize(this);
	MouseBehavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY));
	AddInputBehavior(MouseBehavior);
}

void UTransformGizmo::SetupMaterials()
{
	UMaterial* AxisMaterialBase = GEngine->ArrowMaterial;

	AxisMaterialX = UMaterialInstanceDynamic::Create(AxisMaterialBase, NULL);
	AxisMaterialX->SetVectorParameterValue("GizmoColor", AxisColorX);

	AxisMaterialY = UMaterialInstanceDynamic::Create(AxisMaterialBase, NULL);
	AxisMaterialY->SetVectorParameterValue("GizmoColor", AxisColorY);

	AxisMaterialZ = UMaterialInstanceDynamic::Create(AxisMaterialBase, NULL);
	AxisMaterialZ->SetVectorParameterValue("GizmoColor", AxisColorZ);

	GreyMaterial = UMaterialInstanceDynamic::Create(AxisMaterialBase, NULL);
	GreyMaterial->SetVectorParameterValue("GizmoColor", GreyColor);

	WhiteMaterial = UMaterialInstanceDynamic::Create(AxisMaterialBase, NULL);
	WhiteMaterial->SetVectorParameterValue("GizmoColor", WhiteColor);

	CurrentAxisMaterial = UMaterialInstanceDynamic::Create(AxisMaterialBase, NULL);
	CurrentAxisMaterial->SetVectorParameterValue("GizmoColor", CurrentColor);

	OpaquePlaneMaterialXY = UMaterialInstanceDynamic::Create(AxisMaterialBase, NULL);
	OpaquePlaneMaterialXY->SetVectorParameterValue("GizmoColor", FLinearColor::White);

	TransparentVertexColorMaterial = (UMaterial*)StaticLoadObject(
		UMaterial::StaticClass(), NULL,
		TEXT("/Engine/EditorMaterials/WidgetVertexColorMaterial.WidgetVertexColorMaterial"), NULL, LOAD_None, NULL);

	GridMaterial = (UMaterial*)StaticLoadObject(
		UMaterial::StaticClass(), NULL,
		TEXT("/Engine/EditorMaterials/WidgetGridVertexColorMaterial_Ma.WidgetGridVertexColorMaterial_Ma"), NULL,
		LOAD_None, NULL);
	if (!GridMaterial)
	{
		GridMaterial = TransparentVertexColorMaterial;
	}
}

void UTransformGizmo::Shutdown()
{
	ClearActiveTarget();
}

FTransform UTransformGizmo::GetGizmoTransform() const
{
	float Scale = 1.0f;

	if (TransformGizmoSource)
	{
		Scale = TransformGizmoSource->GetGizmoScale();
	}

	FTransform GizmoLocalToWorldTransform = CurrentTransform;
	GizmoLocalToWorldTransform.SetScale3D(FVector(Scale, Scale, Scale));

	return GizmoLocalToWorldTransform;
}

void UTransformGizmo::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (bVisible && GizmoElementRoot && RenderAPI)
	{
		CurrentTransform = ActiveTarget->GetTransform();

		UGizmoElementBase::FRenderTraversalState RenderState;
		RenderState.Initialize(RenderAPI->GetSceneView(), GetGizmoTransform());
		GizmoElementRoot->Render(RenderAPI, RenderState);
	}
}

FInputRayHit UTransformGizmo::BeginHoverSequenceHitTest(const FInputDeviceRay& DevicePos)
{
	return UpdateHoveredPart(DevicePos);
}

void UTransformGizmo::OnBeginHover(const FInputDeviceRay& DevicePos)
{
}

bool UTransformGizmo::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	FInputRayHit RayHit = UpdateHoveredPart(DevicePos);
	return RayHit.bHit;
}

void UTransformGizmo::OnEndHover()
{
	if (HitTarget && LastHitPart != ETransformGizmoPartIdentifier::Default)
	{
		HitTarget->UpdateHoverState(false, static_cast<uint32>(LastHitPart));
	}
}

FInputRayHit UTransformGizmo::UpdateHoveredPart(const FInputDeviceRay& PressPos)
{
	if (!HitTarget)
	{
		return FInputRayHit();
	}

	FInputRayHit RayHit = HitTarget->IsHit(PressPos);

	ETransformGizmoPartIdentifier HitPart;
	if (RayHit.bHit && VerifyPartIdentifier(RayHit.HitIdentifier))
	{
		HitPart = static_cast<ETransformGizmoPartIdentifier>(RayHit.HitIdentifier);
	}
	else
	{
		HitPart = ETransformGizmoPartIdentifier::Default;
	}

	if (HitPart != LastHitPart)
	{
		if (LastHitPart != ETransformGizmoPartIdentifier::Default)
		{
			UpdateHoverState(false, LastHitPart);
		}

		if (HitPart != ETransformGizmoPartIdentifier::Default)
		{
			UpdateHoverState(true, HitPart);
		}

		LastHitPart = HitPart;
	}

	return RayHit;
}

uint32 UTransformGizmo::GetMaxPartIdentifier() const
{
	return static_cast<uint32>(ETransformGizmoPartIdentifier::Max);
}

bool UTransformGizmo::VerifyPartIdentifier(uint32 InPartIdentifier) const
{
	if (InPartIdentifier >= GetMaxPartIdentifier())
	{
		UE_LOG(LogTransformGizmo, Warning, TEXT("Unrecognized transform gizmo part identifier %d, valid identifiers are between 0-%d."), 
			InPartIdentifier, GetMaxPartIdentifier());
		return false;
	}

	return true;
}



FInputRayHit UTransformGizmo::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FInputRayHit RayHit;

	if (HitTarget)
	{
		RayHit = HitTarget->IsHit(PressPos);
		ETransformGizmoPartIdentifier HitPart;
		if (RayHit.bHit && VerifyPartIdentifier(RayHit.HitIdentifier))
		{
			HitPart = static_cast<ETransformGizmoPartIdentifier>(RayHit.HitIdentifier);
		}
		else
		{
			HitPart = ETransformGizmoPartIdentifier::Default;
		}

		if (HitPart != ETransformGizmoPartIdentifier::Default)
		{
			LastHitPart = static_cast<ETransformGizmoPartIdentifier>(RayHit.HitIdentifier);
		}
	}

	return RayHit;
}

void UTransformGizmo::UpdateMode()
{
	if (TransformGizmoSource && TransformGizmoSource->GetVisible())
	{
		EGizmoTransformMode NewMode = TransformGizmoSource->GetGizmoMode();
		EAxisList::Type NewAxisToDraw = TransformGizmoSource->GetGizmoAxisToDraw(NewMode);

		if (NewMode != CurrentMode)
		{
			EnableMode(CurrentMode, EAxisList::None);
			EnableMode(NewMode, NewAxisToDraw);

			CurrentMode = NewMode;
			CurrentAxisToDraw = NewAxisToDraw;
		}
		else if (NewAxisToDraw != CurrentAxisToDraw)
		{
			EnableMode(CurrentMode, NewAxisToDraw);
			CurrentAxisToDraw = NewAxisToDraw;
		}
	}
	else
	{
		EnableMode(CurrentMode, EAxisList::None);
		CurrentMode = EGizmoTransformMode::None;
	}
}

void UTransformGizmo::EnableMode(EGizmoTransformMode InMode, EAxisList::Type InAxisListToDraw)
{
	if (InMode == EGizmoTransformMode::Translate)
	{
		EnableTranslate(InAxisListToDraw);
	}
	else if (InMode == EGizmoTransformMode::Rotate)
	{
		EnableRotate(InAxisListToDraw);
	}
	else if (InMode == EGizmoTransformMode::Scale)
	{
		EnableScale(InAxisListToDraw);
	}
}

void UTransformGizmo::EnableTranslate(EAxisList::Type InAxisListToDraw)
{
	check(GizmoElementRoot);

	const bool bEnableX = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::X);
	const bool bEnableY = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::Y);
	const bool bEnableZ = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::Z);
	const bool bEnableAny = bEnableX || bEnableY || bEnableZ;

	if (bEnableX && TranslateXAxisElement == nullptr)
	{
		TranslateXAxisElement = MakeTranslateAxis(ETransformGizmoPartIdentifier::TranslateXAxis, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), AxisMaterialX);
		GizmoElementRoot->Add(TranslateXAxisElement);
	}

	if (bEnableY && TranslateYAxisElement == nullptr)
	{
		TranslateYAxisElement = MakeTranslateAxis(ETransformGizmoPartIdentifier::TranslateYAxis, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), AxisMaterialY);
		GizmoElementRoot->Add(TranslateYAxisElement);
	}

	if (bEnableZ && TranslateZAxisElement == nullptr)
	{
		TranslateZAxisElement = MakeTranslateAxis(ETransformGizmoPartIdentifier::TranslateZAxis, FVector(0.0f, 0.0f, 1.0f), FVector(1.0f, 0.0f, 0.0f), AxisMaterialZ);
		GizmoElementRoot->Add(TranslateZAxisElement);
	}

	if (bEnableAny && TranslateScreenSpaceElement == nullptr)
	{
		TranslateScreenSpaceElement = MakeTranslateScreenSpaceHandle();
		GizmoElementRoot->Add(TranslateScreenSpaceElement);
	}

	if (TranslateXAxisElement)
	{
		TranslateXAxisElement->SetEnabled(bEnableX);
	}

	if (TranslateYAxisElement)
	{
		TranslateYAxisElement->SetEnabled(bEnableY);
	}

	if (TranslateZAxisElement)
	{
		TranslateZAxisElement->SetEnabled(bEnableZ);
	}

	if (TranslateScreenSpaceElement)
	{
		TranslateScreenSpaceElement->SetEnabled(bEnableAny);
	}

	EnablePlanarObjects(true, bEnableX, bEnableY, bEnableZ);
}

void UTransformGizmo::EnablePlanarObjects(bool bTranslate, bool bEnableX, bool bEnableY, bool bEnableZ)
{
	check(GizmoElementRoot);

	auto EnablePlanarElement = [this](
		TObjectPtr<UGizmoElementRectangle>& PlanarElement,
		ETransformGizmoPartIdentifier PartId,
		const FVector& Axis0,
		const FVector& Axis1,
		const FVector& Axis2,
		const FLinearColor& AxisColor,
		bool bEnable)
	{
		if (bEnable && PlanarElement == nullptr)
		{
			PlanarElement = MakePlanarHandle(PartId, Axis0, Axis1, Axis2, TransparentVertexColorMaterial, AxisColor);
			GizmoElementRoot->Add(PlanarElement);
		}

		if (PlanarElement)
		{
			PlanarElement->SetEnabled(bEnable);
		}
	};

	const bool bEnableXY = bEnableX && bEnableY;
	const bool bEnableYZ = bEnableY && bEnableZ;
	const bool bEnableXZ = bEnableX && bEnableZ;

	const FVector XAxis(1.0f, 0.0f, 0.0f);
	const FVector YAxis(0.0f, 1.0f, 0.0f);
	const FVector ZAxis(0.0f, 0.0f, 1.0f);

	if (bTranslate)
	{
		EnablePlanarElement(TranslatePlanarXYElement, ETransformGizmoPartIdentifier::TranslateXYPlanar, XAxis, YAxis, ZAxis, AxisColorZ, bEnableXY);
		EnablePlanarElement(TranslatePlanarYZElement, ETransformGizmoPartIdentifier::TranslateYZPlanar, YAxis, ZAxis, XAxis, AxisColorX, bEnableYZ);
		EnablePlanarElement(TranslatePlanarXZElement, ETransformGizmoPartIdentifier::TranslateXZPlanar, ZAxis, XAxis, YAxis, AxisColorY, bEnableXZ);
	}
	else
	{
		EnablePlanarElement(ScalePlanarXYElement, ETransformGizmoPartIdentifier::ScaleXYPlanar, XAxis, YAxis, ZAxis, AxisColorZ, bEnableXY);
		EnablePlanarElement(ScalePlanarYZElement, ETransformGizmoPartIdentifier::ScaleYZPlanar, YAxis, ZAxis, XAxis, AxisColorX, bEnableYZ);
		EnablePlanarElement(ScalePlanarXZElement, ETransformGizmoPartIdentifier::ScaleXZPlanar, ZAxis, XAxis, YAxis, AxisColorY, bEnableXZ);
	}
}

void UTransformGizmo::EnableRotate(EAxisList::Type InAxisListToDraw)
{
	const bool bEnableX = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::X);
	const bool bEnableY = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::Y);
	const bool bEnableZ = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::Z);
	const bool bEnableAll = bEnableX && bEnableY && bEnableZ;

	const FVector XAxis(1.0f, 0.0f, 0.0f);
	const FVector YAxis(0.0f, 1.0f, 0.0f);
	const FVector ZAxis(0.0f, 0.0f, 1.0f);

	if (bEnableX && RotateXAxisElement == nullptr)
	{
		RotateXAxisElement = MakeRotateAxis(ETransformGizmoPartIdentifier::RotateXAxis, YAxis, ZAxis, AxisMaterialX, CurrentAxisMaterial);
		GizmoElementRoot->Add(RotateXAxisElement);
	}

	if (bEnableY && RotateYAxisElement == nullptr)
	{
		RotateYAxisElement = MakeRotateAxis(ETransformGizmoPartIdentifier::RotateYAxis, ZAxis, XAxis, AxisMaterialY, CurrentAxisMaterial);
		GizmoElementRoot->Add(RotateYAxisElement);
	}

	if (bEnableZ && RotateZAxisElement == nullptr)
	{
		RotateZAxisElement = MakeRotateAxis(ETransformGizmoPartIdentifier::RotateZAxis, XAxis, YAxis, AxisMaterialZ, CurrentAxisMaterial);
		GizmoElementRoot->Add(RotateZAxisElement);
	}

	if (bEnableAll)
	{
		if (RotateScreenSpaceElement == nullptr)
		{
			RotateScreenSpaceElement = MakeRotateCircleHandle(ETransformGizmoPartIdentifier::RotateScreenSpace, RotateScreenSpaceRadius, RotateScreenSpaceCircleColor, false);
			GizmoElementRoot->Add(RotateScreenSpaceElement);
		}

		if (RotateOuterCircleElement == nullptr)
		{
			RotateOuterCircleElement = MakeRotateCircleHandle(ETransformGizmoPartIdentifier::Default, RotateOuterCircleRadius, RotateOuterCircleColor, false);
			GizmoElementRoot->Add(RotateOuterCircleElement);
		}

		if (RotateArcballOuterElement == nullptr)
		{
			RotateArcballOuterElement = MakeRotateCircleHandle(ETransformGizmoPartIdentifier::RotateArcball, RotateArcballOuterRadius, RotateArcballCircleColor, false);
			GizmoElementRoot->Add(RotateArcballOuterElement);
		}

		if (RotateArcballInnerElement == nullptr)
		{
			RotateArcballInnerElement = MakeRotateCircleHandle(ETransformGizmoPartIdentifier::RotateArcballInnerCircle, RotateArcballInnerRadius, RotateArcballCircleColor, true);
			GizmoElementRoot->Add(RotateArcballInnerElement);
		}
	}

	if (RotateXAxisElement)
	{
		RotateXAxisElement->SetEnabled(bEnableX);
	}

	if (RotateYAxisElement)
	{
		RotateYAxisElement->SetEnabled(bEnableY);
	}

	if (RotateZAxisElement)
	{
		RotateZAxisElement->SetEnabled(bEnableZ);
	}

	if (RotateScreenSpaceElement)
	{
		RotateScreenSpaceElement->SetEnabled(bEnableAll);
	}

	if (RotateOuterCircleElement)
	{
		RotateOuterCircleElement->SetEnabled(bEnableAll);
	}

	if (RotateArcballOuterElement)
	{
		RotateArcballOuterElement->SetEnabled(bEnableAll);
	}

	if (RotateArcballInnerElement)
	{ 
		RotateArcballInnerElement->SetEnabled(bEnableAll);
	}
}

void UTransformGizmo::EnableScale(EAxisList::Type InAxisListToDraw)
{
	check(GizmoElementRoot);

	bool bEnableX = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::X);
	bool bEnableY = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::Y);
	bool bEnableZ = static_cast<uint8>(InAxisListToDraw) & static_cast<uint8>(EAxisList::Z);
	
	if (bEnableX && ScaleXAxisElement == nullptr)
	{
		ScaleXAxisElement = MakeScaleAxis(ETransformGizmoPartIdentifier::ScaleXAxis, FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), AxisMaterialX);
		GizmoElementRoot->Add(ScaleXAxisElement);
	}

	if (bEnableY && ScaleYAxisElement == nullptr)
	{
		ScaleYAxisElement = MakeScaleAxis(ETransformGizmoPartIdentifier::ScaleYAxis, FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), AxisMaterialY);
		GizmoElementRoot->Add(ScaleYAxisElement);
	}

	if (bEnableZ && ScaleZAxisElement == nullptr)
	{
		ScaleZAxisElement = MakeScaleAxis(ETransformGizmoPartIdentifier::ScaleZAxis, FVector(0.0f, 0.0f, 1.0f), FVector(1.0f, 0.0f, 0.0f), AxisMaterialZ);
		GizmoElementRoot->Add(ScaleZAxisElement);
	}

	if ((bEnableX || bEnableY || bEnableZ) && ScaleUniformElement == nullptr)
	{
		ScaleUniformElement = MakeUniformScaleHandle();
		GizmoElementRoot->Add(ScaleUniformElement);
	}

	if (ScaleXAxisElement)
	{
		ScaleXAxisElement->SetEnabled(bEnableX);
	}

	if (ScaleYAxisElement)
	{
		ScaleYAxisElement->SetEnabled(bEnableY);
	}

	if (ScaleZAxisElement)
	{
		ScaleZAxisElement->SetEnabled(bEnableZ);
	}

	if (ScaleUniformElement)
	{
		ScaleUniformElement->SetEnabled(bEnableX || bEnableY || bEnableZ);
	}

	EnablePlanarObjects(false, bEnableX, bEnableY, bEnableZ);
}

void UTransformGizmo::UpdateCameraAxisSource()
{
	FViewCameraState CameraState;
	GetGizmoManager()->GetContextQueriesAPI()->GetCurrentViewState(CameraState);
	if (CameraAxisSource != nullptr)
	{
		CameraAxisSource->Origin = ActiveTarget ? ActiveTarget->GetTransform().GetLocation() : FVector::ZeroVector;
		CameraAxisSource->Direction = -CameraState.Forward();
		CameraAxisSource->TangentX = CameraState.Right();
		CameraAxisSource->TangentY = CameraState.Up();
	}
}

void UTransformGizmo::Tick(float DeltaTime)
{
	UpdateMode();

	UpdateCameraAxisSource();
}

void UTransformGizmo::SetActiveTarget(UTransformProxy* Target, IToolContextTransactionProvider* TransactionProvider)
{
	if (ActiveTarget != nullptr)
	{
		ClearActiveTarget();
	}

	ActiveTarget = Target;

	// Set current mode to none, mode will be updated next Tick()
	CurrentMode = EGizmoTransformMode::None;

	if (!ActiveTarget)
	{
		return;
	}

	// This state target emits an explicit FChange that moves the GizmoActor root component during undo/redo.
	// It also opens/closes the Transaction that saves/restores the target object locations.
	if (TransactionProvider == nullptr)
	{
		TransactionProvider = GetGizmoManager();
	}
	
	StateTarget = UGizmoObjectModifyStateTarget::Construct(Target,
		LOCTEXT("UTransformGizmoTransaction", "Transform"), TransactionProvider, this);

	CameraAxisSource = NewObject<UGizmoConstantFrameAxisSource>(this);
}

// @todo: This should either be named to "SetScale" or removed, since it can be done with ReinitializeGizmoTransform
void UTransformGizmo::SetNewChildScale(const FVector& NewChildScale)
{
	FTransform NewTransform = ActiveTarget->GetTransform();
	NewTransform.SetScale3D(NewChildScale);

	TGuardValue<bool>(ActiveTarget->bSetPivotMode, true);
	ActiveTarget->SetTransform(NewTransform);
}

void UTransformGizmo::SetVisibility(bool bVisibleIn)
{
	bVisible = bVisibleIn;
}

UGizmoElementArrow* UTransformGizmo::MakeTranslateAxis(ETransformGizmoPartIdentifier InPartId, const FVector& InAxisDir, const FVector& InSideDir, UMaterialInterface* InMaterial)
{
	UGizmoElementArrow* ArrowElement = NewObject<UGizmoElementArrow>();
	ArrowElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	ArrowElement->SetHeadType(EGizmoElementArrowHeadType::Cone);
	ArrowElement->SetBase(InAxisDir * AxisLengthOffset);
	ArrowElement->SetDirection(InAxisDir);
	ArrowElement->SetSideDirection(InSideDir);
	ArrowElement->SetBodyLength(TranslateAxisLength);
	ArrowElement->SetBodyRadius(AxisRadius);
	ArrowElement->SetHeadLength(TranslateAxisConeHeight);
	ArrowElement->SetHeadRadius(TranslateAxisConeRadius);
	ArrowElement->SetNumSides(32);
	ArrowElement->SetMaterial(InMaterial);
	ArrowElement->SetViewDependentType(EGizmoElementViewDependentType::Axis);
	ArrowElement->SetViewDependentAxis(InAxisDir);
	return ArrowElement;
}

UGizmoElementArrow* UTransformGizmo::MakeScaleAxis(ETransformGizmoPartIdentifier InPartId, const FVector& InAxisDir, const FVector& InSideDir, UMaterialInterface* InMaterial)
{
	UGizmoElementArrow* ArrowElement = NewObject<UGizmoElementArrow>();
	ArrowElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	ArrowElement->SetHeadType(EGizmoElementArrowHeadType::Cube);
	ArrowElement->SetBase(InAxisDir * AxisLengthOffset);
	ArrowElement->SetDirection(InAxisDir);
	ArrowElement->SetSideDirection(InSideDir);
	ArrowElement->SetBodyLength(ScaleAxisLength);
	ArrowElement->SetBodyRadius(AxisRadius);
	ArrowElement->SetHeadLength(ScaleAxisCubeDim);
	ArrowElement->SetNumSides(32);
	ArrowElement->SetMaterial(InMaterial);
	ArrowElement->SetViewDependentType(EGizmoElementViewDependentType::Axis);
	ArrowElement->SetViewDependentAxis(InAxisDir);
	return ArrowElement;
}

UGizmoElementBox* UTransformGizmo::MakeUniformScaleHandle()
{
	UGizmoElementBox* BoxElement = NewObject<UGizmoElementBox>();
	BoxElement->SetPartIdentifier(static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleUniform));
	BoxElement->SetCenter(FVector::ZeroVector);
	BoxElement->SetUpDirection(FVector::UpVector);
	BoxElement->SetSideDirection(FVector::RightVector);
	BoxElement->SetDimensions(FVector(ScaleAxisCubeDim, ScaleAxisCubeDim, ScaleAxisCubeDim));
	BoxElement->SetMaterial(GreyMaterial);
	return BoxElement;
}

UGizmoElementRectangle* UTransformGizmo::MakePlanarHandle(ETransformGizmoPartIdentifier InPartId, const FVector& InUpDirection, const FVector& InSideDirection, const FVector& InPlaneNormal,
	UMaterialInterface* InMaterial, const FLinearColor& InVertexColor)
{
	FVector PlanarHandleCenter = (InUpDirection + InSideDirection) * PlanarHandleOffset;

	FLinearColor LineColor = InVertexColor;
	FLinearColor VertexColor = LineColor;
	VertexColor.A = LargeOuterAlpha;

	UGizmoElementRectangle* RectangleElement = NewObject<UGizmoElementRectangle>();
	RectangleElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	RectangleElement->SetUpDirection(InUpDirection);
	RectangleElement->SetSideDirection(InSideDirection);
	RectangleElement->SetCenter(PlanarHandleCenter);
	RectangleElement->SetHeight(PlanarHandleSize);
	RectangleElement->SetWidth(PlanarHandleSize);
	RectangleElement->SetMaterial(InMaterial);
	RectangleElement->SetVertexColor(VertexColor);
	RectangleElement->SetLineColor(LineColor);
	RectangleElement->SetDrawLine(true);
	RectangleElement->SetDrawMesh(true);
	RectangleElement->SetHitMesh(true);
	RectangleElement->SetViewDependentType(EGizmoElementViewDependentType::Plane);
	RectangleElement->SetViewDependentAxis(InPlaneNormal);
	return RectangleElement;
}

UGizmoElementRectangle* UTransformGizmo::MakeTranslateScreenSpaceHandle()
{
	UGizmoElementRectangle* RectangleElement = NewObject<UGizmoElementRectangle>();
	RectangleElement->SetPartIdentifier(static_cast<uint32>(ETransformGizmoPartIdentifier::TranslateScreenSpace));
	RectangleElement->SetUpDirection(FVector::UpVector);
	RectangleElement->SetSideDirection(FVector::RightVector);
	RectangleElement->SetCenter(FVector::ZeroVector);
	RectangleElement->SetHeight(TranslateScreenSpaceHandleSize);
	RectangleElement->SetWidth(TranslateScreenSpaceHandleSize);
	RectangleElement->SetViewAlignType(EGizmoElementViewAlignType::PointScreen);
	RectangleElement->SetViewAlignAxis(FVector::UpVector);
	RectangleElement->SetViewAlignNormal(-FVector::ForwardVector);
	RectangleElement->SetMaterial(TransparentVertexColorMaterial);
	RectangleElement->SetLineColor(ScreenSpaceColor);
	RectangleElement->SetHitMesh(true);
	RectangleElement->SetDrawMesh(false);
	RectangleElement->SetDrawLine(true);
	RectangleElement->SetHoverLineThicknessMultiplier(3.0f);
	RectangleElement->SetInteractLineThicknessMultiplier(3.0f);
	return RectangleElement;
}

UGizmoElementTorus* UTransformGizmo::MakeRotateAxis(ETransformGizmoPartIdentifier InPartId, const FVector& TorusAxis0, const FVector& TorusAxis1,
	UMaterialInterface* InMaterial, UMaterialInterface* InCurrentMaterial)
{
	UGizmoElementTorus* RotateAxisElement = NewObject<UGizmoElementTorus>();
	RotateAxisElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	RotateAxisElement->SetCenter(FVector::ZeroVector);
	RotateAxisElement->SetRadius(UTransformGizmo::RotateAxisOuterRadius);
	RotateAxisElement->SetNumSegments(UTransformGizmo::RotateAxisNumSegments);
	RotateAxisElement->SetInnerRadius(UTransformGizmo::RotateAxisInnerRadius);
	RotateAxisElement->SetNumInnerSlices(UTransformGizmo::RotateAxisInnerSlices);
	RotateAxisElement->SetAxis0(TorusAxis0);
	RotateAxisElement->SetAxis1(TorusAxis1);
	const FVector TorusNormal = RotateAxisElement->GetAxis0() ^ RotateAxisElement->GetAxis1();
	RotateAxisElement->SetPartialType(EGizmoElementPartialType::PartialViewDependent);
	RotateAxisElement->SetPartialStartAngle(0.0f);
	RotateAxisElement->SetPartialEndAngle(UE_PI);
	RotateAxisElement->SetViewDependentAxis(TorusNormal);
	RotateAxisElement->SetViewAlignType(EGizmoElementViewAlignType::Axial);
	RotateAxisElement->SetViewAlignAxis(TorusNormal);
	RotateAxisElement->SetViewAlignNormal(TorusAxis1);
	RotateAxisElement->SetMaterial(InMaterial);
	return RotateAxisElement;
}

UGizmoElementCircle* UTransformGizmo::MakeRotateCircleHandle(ETransformGizmoPartIdentifier InPartId, float InRadius, const FLinearColor& InColor, float bFill)
{
	UGizmoElementCircle* CircleElement = NewObject<UGizmoElementCircle>();
	CircleElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	CircleElement->SetCenter(FVector::ZeroVector);
	CircleElement->SetRadius(InRadius);
	CircleElement->SetAxis0(FVector::UpVector);
	CircleElement->SetAxis1(-FVector::RightVector);
	CircleElement->SetLineColor(InColor);
	CircleElement->SetViewAlignType(EGizmoElementViewAlignType::PointOnly);
	CircleElement->SetViewAlignNormal(-FVector::ForwardVector);

	if (bFill)
	{
		CircleElement->SetVertexColor(InColor);
		CircleElement->SetMaterial(WhiteMaterial);
	}
	else
	{
		CircleElement->SetDrawLine(true);
		CircleElement->SetHitLine(true);
		CircleElement->SetDrawMesh(false);
		CircleElement->SetHitMesh(false);
	}

	return CircleElement;
}


void UTransformGizmo::ClearActiveTarget()
{
	StateTarget = nullptr;
	ActiveTarget = nullptr;
}


bool UTransformGizmo::PositionSnapFunction(const FVector& WorldPosition, FVector& SnappedPositionOut) const
{
	SnappedPositionOut = WorldPosition;
#if 0
	// only snap if we want snapping obvs
	if (bSnapToWorldGrid == false)
	{
		return false;
	}

	// only snap to world grid when using world axes
	if (GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem() != EToolContextCoordinateSystem::World)
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
	if (GetGizmoManager()->GetContextQueriesAPI()->ExecuteSceneSnapQuery(Request, Results))
	{
		SnappedPositionOut = Results[0].Position;
		return true;
	};
#endif
	return false;
}


FQuat UTransformGizmo::RotationSnapFunction(const FQuat& DeltaRotation) const
{
	FQuat SnappedDeltaRotation = DeltaRotation;
#if 0
	// only snap if we want snapping 
	if (bSnapToWorldRotGrid)
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
		if (GetGizmoManager()->GetContextQueriesAPI()->ExecuteSceneSnapQuery(Request, Results))
		{
			SnappedDeltaRotation = Results[0].DeltaRotation;
		};
	}
#endif	
	return SnappedDeltaRotation;
}

FVector UTransformGizmo::GetWorldAxis(const FVector& InAxis)
{
	if (TransformGizmoSource->GetGizmoCoordSystemSpace() == EToolContextCoordinateSystem::Local)
	{
		return CurrentTransform.TransformVectorNoScale(InAxis);
	}
	
	return InAxis;
}


void UTransformGizmo::SetupOnClickFunctions()
{
	int NumParts = static_cast<int>(ETransformGizmoPartIdentifier::Max);
	OnClickPressFunctions.SetNum(NumParts);
	OnClickDragFunctions.SetNum(NumParts);
	OnClickReleaseFunctions.SetNum(NumParts);

	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXAxis)] = &UTransformGizmo::OnClickPressTranslateXAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateYAxis)] = &UTransformGizmo::OnClickPressTranslateYAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateZAxis)] = &UTransformGizmo::OnClickPressTranslateZAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXYPlanar)] = &UTransformGizmo::OnClickPressTranslateXYPlanar;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateYZPlanar)] = &UTransformGizmo::OnClickPressTranslateYZPlanar;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXZPlanar)] = &UTransformGizmo::OnClickPressTranslateXZPlanar;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateScreenSpace)] = &UTransformGizmo::OnClickPressScreenSpaceTranslate;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXAxis)] = &UTransformGizmo::OnClickPressScaleXAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleYAxis)] = &UTransformGizmo::OnClickPressScaleYAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleZAxis)] = &UTransformGizmo::OnClickPressScaleZAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXYPlanar)] = &UTransformGizmo::OnClickPressScaleXYPlanar;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleYZPlanar)] = &UTransformGizmo::OnClickPressScaleYZPlanar;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXZPlanar)] = &UTransformGizmo::OnClickPressScaleXZPlanar;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleUniform)] = &UTransformGizmo::OnClickPressScaleXYZ;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateXAxis)] = &UTransformGizmo::OnClickPressRotateXAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateYAxis)] = &UTransformGizmo::OnClickPressRotateYAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateZAxis)] = &UTransformGizmo::OnClickPressRotateZAxis;
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateScreenSpace)] = &UTransformGizmo::OnClickPressScreenSpaceRotate;

	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXAxis)] = &UTransformGizmo::OnClickDragTranslateAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateYAxis)] = &UTransformGizmo::OnClickDragTranslateAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateZAxis)] = &UTransformGizmo::OnClickDragTranslateAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXYPlanar)] = &UTransformGizmo::OnClickDragTranslatePlanar;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateYZPlanar)] = &UTransformGizmo::OnClickDragTranslatePlanar;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXZPlanar)] = &UTransformGizmo::OnClickDragTranslatePlanar;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateScreenSpace)] = &UTransformGizmo::OnClickDragScreenSpaceTranslate;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXAxis)] = &UTransformGizmo::OnClickDragScaleAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleYAxis)] = &UTransformGizmo::OnClickDragScaleAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleZAxis)] = &UTransformGizmo::OnClickDragScaleAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXYPlanar)] = &UTransformGizmo::OnClickDragScalePlanar;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleYZPlanar)] = &UTransformGizmo::OnClickDragScalePlanar;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXZPlanar)] = &UTransformGizmo::OnClickDragScalePlanar;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleUniform)] = &UTransformGizmo::OnClickDragScaleXYZ;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateXAxis)] = &UTransformGizmo::OnClickDragRotateAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateYAxis)] = &UTransformGizmo::OnClickDragRotateAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateZAxis)] = &UTransformGizmo::OnClickDragRotateAxis;
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateScreenSpace)] = &UTransformGizmo::OnClickDragScreenSpaceRotate;

	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXAxis)] = &UTransformGizmo::OnClickReleaseTranslateAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateYAxis)] = &UTransformGizmo::OnClickReleaseTranslateAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateZAxis)] = &UTransformGizmo::OnClickReleaseTranslateAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXYPlanar)] = &UTransformGizmo::OnClickReleaseTranslatePlanar;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateYZPlanar)] = &UTransformGizmo::OnClickReleaseTranslatePlanar;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateXZPlanar)] = &UTransformGizmo::OnClickReleaseTranslatePlanar;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::TranslateScreenSpace)] = &UTransformGizmo::OnClickReleaseScreenSpaceTranslate;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXAxis)] = &UTransformGizmo::OnClickReleaseScaleAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleYAxis)] = &UTransformGizmo::OnClickReleaseScaleAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleZAxis)] = &UTransformGizmo::OnClickReleaseScaleAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXYPlanar)] = &UTransformGizmo::OnClickReleaseScalePlanar;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleYZPlanar)] = &UTransformGizmo::OnClickReleaseScalePlanar;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleXZPlanar)] = &UTransformGizmo::OnClickReleaseScalePlanar;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::ScaleUniform)] = &UTransformGizmo::OnClickReleaseScaleXYZ;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateXAxis)] = &UTransformGizmo::OnClickReleaseRotateAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateYAxis)] = &UTransformGizmo::OnClickReleaseRotateAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateZAxis)] = &UTransformGizmo::OnClickReleaseRotateAxis;
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateScreenSpace)] = &UTransformGizmo::OnClickReleaseScreenSpaceRotate;
}


float UTransformGizmo::GetNearestRayParamToInteractionAxis(const FInputDeviceRay& InRay)
{
	float RayNearestParam, AxisNearestParam;
	FVector RayNearestPt, AxisNearestPoint;
	GizmoMath::NearestPointOnLineToRay(InteractionAxisOrigin, InteractionAxisDirection,
		InRay.WorldRay.Origin, InRay.WorldRay.Direction, AxisNearestPoint, AxisNearestParam,
		RayNearestPt, RayNearestParam);
	return AxisNearestParam;
}

bool UTransformGizmo::GetRayParamIntersectionWithInteractionPlane(const FInputDeviceRay& InRay, float& OutHitParam)
{
	// if ray is parallel to plane, nothing has been hit
	if (FMath::IsNearlyZero(FVector::DotProduct(InteractionPlanarNormal, InRay.WorldRay.Direction)))
	{
		return false;
	}

	FPlane Plane(InteractionPlanarOrigin, InteractionPlanarNormal);
	OutHitParam = FMath::RayPlaneIntersectionParam(InRay.WorldRay.Origin, InRay.WorldRay.Direction, Plane);
	if (OutHitParam < 0)
	{
		return false;
	}

	return true;
}

void UTransformGizmo::UpdateHoverState(bool bInHover, ETransformGizmoPartIdentifier InHitPartId)
{
	HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(InHitPartId));

	switch (InHitPartId)
	{
	case ETransformGizmoPartIdentifier::ScaleUniform:
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleXAxis));
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleYAxis));
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleZAxis));
		break;
	case ETransformGizmoPartIdentifier::ScaleXYPlanar:
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleXAxis));
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleYAxis));
		break;
	case ETransformGizmoPartIdentifier::ScaleYZPlanar:
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleYAxis));
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleZAxis));
		break;
	case ETransformGizmoPartIdentifier::ScaleXZPlanar:
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleXAxis));
		HitTarget->UpdateHoverState(bInHover, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleZAxis));
		break;
	}
}

void UTransformGizmo::UpdateInteractingState(bool bInInteracting, ETransformGizmoPartIdentifier InHitPartId)
{
	HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(InHitPartId));

	switch (InHitPartId)
	{
	case ETransformGizmoPartIdentifier::ScaleUniform:
	{
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleXAxis));
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleYAxis));
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleZAxis));
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleUniform));
		GizmoElementRoot->UpdatePartVisibleState(!bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleXYPlanar));
		GizmoElementRoot->UpdatePartVisibleState(!bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleYZPlanar));
		GizmoElementRoot->UpdatePartVisibleState(!bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleXZPlanar));
		break;
	}
	case ETransformGizmoPartIdentifier::ScaleXYPlanar:
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleXAxis));
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleYAxis));
		break;
	case ETransformGizmoPartIdentifier::ScaleYZPlanar:
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleYAxis));
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleZAxis));
		break;
	case ETransformGizmoPartIdentifier::ScaleXZPlanar:
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleXAxis));
		HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleZAxis));
		break;
	}
}

void UTransformGizmo::OnClickPress(const FInputDeviceRay& PressPos)
{
	check(OnClickPressFunctions.Num() == static_cast<int>(ETransformGizmoPartIdentifier::Max));

	if (OnClickPressFunctions[static_cast<int>(LastHitPart)])
	{
		OnClickPressFunctions[static_cast<int>(LastHitPart)](this, PressPos);
	}

	if (bInInteraction)
	{
		if (HitTarget && LastHitPart != ETransformGizmoPartIdentifier::Default)
		{
			UpdateInteractingState(true, LastHitPart);
		}

		if (StateTarget)
		{
			StateTarget->BeginUpdate();
		}
	}
}

void UTransformGizmo::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (!bInInteraction)
	{
		return;
	}

	int HitPartIndex = static_cast<int>(LastHitPart);
	check(HitPartIndex < OnClickDragFunctions.Num());

	if (OnClickDragFunctions[HitPartIndex])
	{
		OnClickDragFunctions[HitPartIndex](this, DragPos);
	}
}

void UTransformGizmo::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	if (!bInInteraction)
	{
		return;
	}

	int HitPartIndex = static_cast<int>(LastHitPart);
	check(HitPartIndex < OnClickReleaseFunctions.Num());

	if (OnClickReleaseFunctions[HitPartIndex])
	{
		OnClickReleaseFunctions[HitPartIndex](this, ReleasePos);
	}

	if (StateTarget)
	{
		StateTarget->EndUpdate();
	}

	bInInteraction = false;

	if (HitTarget && LastHitPart != ETransformGizmoPartIdentifier::Default)
	{
		UpdateInteractingState(false, LastHitPart);
	}
}

void UTransformGizmo::OnTerminateDragSequence()
{
	if (!bInInteraction)
	{
		return;
	}

	if (StateTarget)
	{
		StateTarget->EndUpdate();
	}
	bInInteraction = false;

	if (HitTarget && LastHitPart != ETransformGizmoPartIdentifier::Default)
	{
		HitTarget->UpdateInteractingState(false, static_cast<uint32>(LastHitPart));
	}
}

void UTransformGizmo::OnClickPressTranslateXAxis(const FInputDeviceRay& PressPos)
{
	InteractionAxisOrigin = CurrentTransform.GetLocation();
	InteractionAxisDirection = GetWorldAxis(FVector::XAxisVector);
	InteractionAxisList = EAxisList::X;
	OnClickPressAxis(PressPos);
}

void UTransformGizmo::OnClickPressTranslateYAxis(const FInputDeviceRay& PressPos)
{
	InteractionAxisOrigin = CurrentTransform.GetLocation();
	InteractionAxisDirection = GetWorldAxis(FVector::YAxisVector);
	InteractionAxisList = EAxisList::Y;
	OnClickPressAxis(PressPos);
}

void UTransformGizmo::OnClickPressTranslateZAxis(const FInputDeviceRay& PressPos)
{
	InteractionAxisOrigin = CurrentTransform.GetLocation();
	InteractionAxisDirection = GetWorldAxis(FVector::ZAxisVector);
	InteractionAxisList = EAxisList::Z;
	OnClickPressAxis(PressPos);
}

void UTransformGizmo::OnClickPressAxis(const FInputDeviceRay& PressPos)
{
	InteractionAxisStartParam = GetNearestRayParamToInteractionAxis(PressPos);
	InteractionAxisCurrParam = InteractionAxisStartParam;
	bInInteraction = true;
}

void UTransformGizmo::OnClickDragTranslateAxis(const FInputDeviceRay& DragPos)
{
	float AxisNearestParam = GetNearestRayParamToInteractionAxis(DragPos);
	FVector Delta = ComputeAxisTranslateDelta(InteractionAxisCurrParam, AxisNearestParam);
	ApplyTranslateDelta(Delta);
	InteractionAxisCurrParam = AxisNearestParam;
}

void UTransformGizmo::OnClickReleaseTranslateAxis(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

void UTransformGizmo::OnClickPressTranslateXYPlanar(const FInputDeviceRay& PressPos)
{
	InteractionPlanarOrigin = CurrentTransform.GetLocation();
	InteractionPlanarNormal = GetWorldAxis(FVector::ZAxisVector);
	InteractionPlanarAxisX = GetWorldAxis(FVector::XAxisVector);
	InteractionPlanarAxisY = GetWorldAxis(FVector::YAxisVector);
	InteractionAxisList = EAxisList::XY;
	OnClickPressPlanar(PressPos);
}

void UTransformGizmo::OnClickPressTranslateYZPlanar(const FInputDeviceRay& PressPos)
{
	InteractionPlanarOrigin = CurrentTransform.GetLocation();
	InteractionPlanarNormal = GetWorldAxis(FVector::XAxisVector);
	InteractionPlanarAxisX = GetWorldAxis(FVector::YAxisVector);
	InteractionPlanarAxisY = GetWorldAxis(FVector::ZAxisVector);
	InteractionAxisList = EAxisList::YZ;
	OnClickPressPlanar(PressPos);
}

void UTransformGizmo::OnClickPressTranslateXZPlanar(const FInputDeviceRay& PressPos)
{
	InteractionPlanarOrigin = CurrentTransform.GetLocation();
	InteractionPlanarNormal = GetWorldAxis(FVector::YAxisVector);
	InteractionPlanarAxisX = GetWorldAxis(FVector::ZAxisVector);
	InteractionPlanarAxisY = GetWorldAxis(FVector::XAxisVector);
	InteractionAxisList = EAxisList::XZ;
	OnClickPressPlanar(PressPos);
}

void UTransformGizmo::OnClickPressPlanar(const FInputDeviceRay& PressPos)
{
	float HitDepth;
	if (GetRayParamIntersectionWithInteractionPlane(PressPos, HitDepth))
	{
		InteractionPlanarStartPoint = PressPos.WorldRay.Origin + PressPos.WorldRay.Direction * HitDepth;
		InteractionPlanarCurrPoint = InteractionPlanarStartPoint;
		bInInteraction = true;
	}
}

void UTransformGizmo::OnClickDragTranslatePlanar(const FInputDeviceRay& DragPos)
{
	float HitDepth;
	if (GetRayParamIntersectionWithInteractionPlane(DragPos, HitDepth))
	{
		FVector HitPoint = DragPos.WorldRay.Origin + DragPos.WorldRay.Direction * HitDepth;
		FVector Delta = ComputePlanarTranslateDelta(InteractionPlanarCurrPoint, HitPoint);
		ApplyTranslateDelta(Delta);
		InteractionPlanarCurrPoint = HitPoint;
	}
}

void UTransformGizmo::OnClickReleaseTranslatePlanar(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

FVector UTransformGizmo::ComputeAxisTranslateDelta(double InStartParam, double InEndParam)
{
	const double ParamDelta = InEndParam - InStartParam;
	return InteractionAxisDirection * ParamDelta;
}

FVector UTransformGizmo::ComputePlanarTranslateDelta(const FVector& InStartPoint, const FVector& InEndPoint)
{
	return InEndPoint - InStartPoint;
}

void UTransformGizmo::OnClickPressScreenSpaceTranslate(const FInputDeviceRay& PressPos)
{
	check(GizmoViewContext);

	InteractionPlanarOrigin = CurrentTransform.GetLocation();
	InteractionPlanarNormal = -GizmoViewContext->GetViewDirection();
	InteractionPlanarAxisX = GizmoViewContext->GetViewUp();
	InteractionPlanarAxisY = GizmoViewContext->GetViewRight();
	InteractionAxisList = EAxisList::Screen;
	OnClickPressPlanar(PressPos);
}

void UTransformGizmo::OnClickDragScreenSpaceTranslate(const FInputDeviceRay& DragPos)
{
	OnClickDragTranslatePlanar(DragPos);
}

void UTransformGizmo::OnClickReleaseScreenSpaceTranslate(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

void UTransformGizmo::OnClickPressScaleXAxis(const FInputDeviceRay& PressPos)
{
	InteractionAxisList = EAxisList::X;
	OnClickPressScale(PressPos);
}

void UTransformGizmo::OnClickPressScaleYAxis(const FInputDeviceRay& PressPos)
{
	InteractionAxisList = EAxisList::Y;
	OnClickPressScale(PressPos);
}

void UTransformGizmo::OnClickPressScaleZAxis(const FInputDeviceRay& PressPos)
{
	InteractionAxisList = EAxisList::Z;
	OnClickPressScale(PressPos);
}

void UTransformGizmo::OnClickPressScaleXYPlanar(const FInputDeviceRay& PressPos)
{
	InteractionAxisList = EAxisList::XY;
	OnClickPressScale(PressPos);
}

void UTransformGizmo::OnClickPressScaleYZPlanar(const FInputDeviceRay& PressPos)
{
	InteractionAxisList = EAxisList::YZ;
	OnClickPressScale(PressPos);
}

void UTransformGizmo::OnClickPressScaleXZPlanar(const FInputDeviceRay& PressPos)
{
	InteractionAxisList = EAxisList::XZ;
	OnClickPressScale(PressPos);
}

void UTransformGizmo::OnClickPressScaleXYZ(const FInputDeviceRay& PressPos)
{
	InteractionAxisList = EAxisList::XYZ;
	OnClickPressScale(PressPos);
}

void UTransformGizmo::OnClickPressScale(const FInputDeviceRay& PressPos)
{
	FVector2D AxisDir(0.0, 0.0);

	if (InteractionAxisList & EAxisList::X)
	{
		AxisDir += GetScreenProjectedAxis(GizmoViewContext, FVector::XAxisVector, CurrentTransform);
	}
	if (InteractionAxisList & EAxisList::Y)
	{
		AxisDir += GetScreenProjectedAxis(GizmoViewContext, FVector::YAxisVector, CurrentTransform);
	}
	if (InteractionAxisList & EAxisList::Z)
	{
		AxisDir += GetScreenProjectedAxis(GizmoViewContext, FVector::ZAxisVector, CurrentTransform);
	}

	InteractionScreenAxisDirection = AxisDir.GetSafeNormal();
	InteractionScreenStartPos = InteractionScreenEndPos = InteractionScreenCurrPos = PressPos.ScreenPosition;
	bInInteraction = true;
}

void UTransformGizmo::OnClickDragScaleAxis(const FInputDeviceRay& DragPos)
{
	OnClickDragScale(DragPos);
}

void UTransformGizmo::OnClickDragScalePlanar(const FInputDeviceRay& DragPos)
{
	OnClickDragScale(DragPos);
}

void UTransformGizmo::OnClickDragScaleXYZ(const FInputDeviceRay& DragPos)
{
	OnClickDragScale(DragPos);
}

void UTransformGizmo::OnClickDragScale(const FInputDeviceRay& DragPos)
{
	FVector2D ScreenDelta = DragPos.ScreenPosition - InteractionScreenCurrPos;

	if (TransformGizmoSource->GetScaleType() != EGizmoTransformScaleType::PercentageBased)
	{
		ScreenDelta *= ScaleMultiplier;
	}

	InteractionScreenEndPos += ScreenDelta;

	FVector ScaleDelta = ComputeScaleDelta(InteractionScreenStartPos, InteractionScreenEndPos, ScreenDelta);

	if (ScaleDelta.X != 0.0 || ScaleDelta.Y != 0.0 || ScaleDelta.Z != 0.0)
	{
		ApplyScaleDelta(ScaleDelta);
		InteractionScreenEndPos -= ScreenDelta;
		InteractionScreenCurrPos = DragPos.ScreenPosition;
	}
}

void UTransformGizmo::OnClickReleaseScaleAxis(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

void UTransformGizmo::OnClickReleaseScalePlanar(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

void UTransformGizmo::OnClickReleaseScaleXYZ(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

FVector UTransformGizmo::ComputeScaleDelta(const FVector2D& InStartPos, const FVector2D& InEndPos, FVector2D& OutScreenDelta)
{
	const FVector2D DragDir = InEndPos - InStartPos;
	const double ScaleDelta = FVector2D::DotProduct(InteractionScreenAxisDirection, DragDir);

	const FVector Scale(
		(InteractionAxisList & EAxisList::X) ? ScaleDelta : 0.0f,
		(InteractionAxisList & EAxisList::Y) ? ScaleDelta : 0.0f,
		(InteractionAxisList & EAxisList::Z) ? ScaleDelta : 0.0f);

	const float ScaleMax = Scale.GetMax();
	const float ScaleMin = Scale.GetMin();
	const float ScaleApplied = (ScaleMax > -ScaleMin) ? ScaleMax : ScaleMin;

	OutScreenDelta = InteractionScreenAxisDirection * ScaleApplied;

	return Scale;
}

void UTransformGizmo::OnClickPressRotateXAxis(const FInputDeviceRay& PressPos)
{
	InteractionScreenAxisDirection = GetScreenRotateAxisDir(FVector::YAxisVector, FVector::ZAxisVector).GetSafeNormal();
	InteractionAxisList = EAxisList::X;
	InteractionScreenStartPos = InteractionScreenCurrPos = PressPos.ScreenPosition;
	bInInteraction = true;
}

void UTransformGizmo::OnClickPressRotateYAxis(const FInputDeviceRay& PressPos)
{
	InteractionScreenAxisDirection = GetScreenRotateAxisDir(FVector::ZAxisVector, FVector::XAxisVector).GetSafeNormal();
	InteractionAxisList = EAxisList::Y;
	InteractionScreenStartPos = InteractionScreenCurrPos = PressPos.ScreenPosition;
	bInInteraction = true;
}

void UTransformGizmo::OnClickPressRotateZAxis(const FInputDeviceRay& PressPos)
{
	InteractionScreenAxisDirection = GetScreenRotateAxisDir(FVector::XAxisVector, FVector::YAxisVector).GetSafeNormal();
	InteractionAxisList = EAxisList::Z;
	InteractionScreenStartPos = InteractionScreenCurrPos = PressPos.ScreenPosition;
	bInInteraction = true;
}

FVector2D UTransformGizmo::GetScreenRotateAxisDir(const FVector& InAxis0, const FVector& InAxis1)
{
	check(GizmoViewContext);
	const FVector DirectionToWidget = CurrentTransform.GetLocation() - GizmoViewContext->ViewLocation;

	const FVector Axis0 = GetWorldAxis(InAxis0);
	const FVector Axis1 = GetWorldAxis(InAxis1);

	// Reverse the axes based on camera view
	const bool bMirrorAxis0 = (FVector::DotProduct(Axis0, DirectionToWidget) <= 0.0f);
	const bool bMirrorAxis1 = (FVector::DotProduct(Axis1, DirectionToWidget) <= 0.0f);
	const float Direction = (bMirrorAxis0 ^ bMirrorAxis1) ? -1.0f : 1.0f;

	const FVector AxisDir = (Axis1 - Axis0) * Direction;

	return GetScreenProjectedAxis(GizmoViewContext, AxisDir);
}

void UTransformGizmo::OnClickDragRotateAxis(const FInputDeviceRay& DragPos)
{
	FQuat DeltaRot = ComputeAxisRotateDelta(InteractionScreenCurrPos, DragPos.ScreenPosition);
	ApplyRotateDelta(DeltaRot);
	InteractionScreenCurrPos = DragPos.ScreenPosition;
}

FQuat UTransformGizmo::ComputeAxisRotateDelta(const FVector2D& InStartPos, const FVector2D& InEndPos)
{
	FVector2D DragDir = InEndPos - InStartPos;
	FRotator DeltaRot(0.0, 0.0, 0.0);
	if (InteractionAxisList == EAxisList::X)
	{
		DeltaRot.Roll = FVector2D::DotProduct(InteractionScreenAxisDirection, DragDir);
	}
	else if (InteractionAxisList == EAxisList::Y)
	{
		DeltaRot.Pitch = FVector2D::DotProduct(InteractionScreenAxisDirection, DragDir);
	}
	else
	{
		DeltaRot.Yaw = FVector2D::DotProduct(InteractionScreenAxisDirection, DragDir);
	}

	check(TransformGizmoSource);
	if (TransformGizmoSource->GetGizmoCoordSystemSpace() == EToolContextCoordinateSystem::Local)
	{
		check(ActiveTarget);
		FMatrix CurrCoordSystem = ActiveTarget->GetTransform().ToMatrixNoScale();
		DeltaRot = (CurrCoordSystem.Inverse() * FRotationMatrix(DeltaRot) * CurrCoordSystem).Rotator();
	}

	return DeltaRot.Quaternion();
}

void UTransformGizmo::OnClickReleaseRotateAxis(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

void UTransformGizmo::OnClickPressScreenSpaceRotate(const FInputDeviceRay& PressPos)
{
	check(GizmoViewContext);
	
	InteractionPlanarOrigin = CurrentTransform.GetLocation();
	InteractionPlanarNormal = -GizmoViewContext->GetViewDirection();
	InteractionPlanarAxisX = GizmoViewContext->GetViewUp();
	InteractionPlanarAxisY = GizmoViewContext->GetViewRight();
	InteractionAxisList = EAxisList::Screen;

	float HitDepth;
	if (GetRayParamIntersectionWithInteractionPlane(PressPos, HitDepth))
	{
		FVector HitPoint = PressPos.WorldRay.Origin + PressPos.WorldRay.Direction * HitDepth;
		InteractionStartAngle = GizmoMath::ComputeAngleInPlane(HitPoint,
			InteractionPlanarOrigin, InteractionPlanarNormal, InteractionPlanarAxisX, InteractionPlanarAxisY);
		InteractionCurrAngle = InteractionStartAngle;

		bInInteraction = true;
	}
}

void UTransformGizmo::OnClickDragScreenSpaceRotate(const FInputDeviceRay& DragPos)
{
	check(GizmoViewContext);
	
	float HitDepth;
	if (GetRayParamIntersectionWithInteractionPlane(DragPos, HitDepth))
	{
		FVector HitPoint = DragPos.WorldRay.Origin + DragPos.WorldRay.Direction * HitDepth;
		float HitAngle = GizmoMath::ComputeAngleInPlane(HitPoint,
			InteractionPlanarOrigin, InteractionPlanarNormal, InteractionPlanarAxisX, InteractionPlanarAxisY);

		FQuat Delta = ComputeAngularRotateDelta(InteractionCurrAngle, HitAngle);
		ApplyRotateDelta(Delta);
		InteractionCurrAngle = HitAngle;
	}
}

FQuat UTransformGizmo::ComputeAngularRotateDelta(double InStartAngle, double InEndAngle)
{
	float DeltaAngle = InEndAngle - InStartAngle;
	return FQuat(InteractionPlanarNormal, DeltaAngle);
}

void UTransformGizmo::OnClickReleaseScreenSpaceRotate(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

FVector2D UTransformGizmo::GetScreenProjectedAxis(const UGizmoViewContext* View, const FVector& InLocalAxis, const FTransform& InLocalToWorld) const
{
	FVector2D Origin;
	FVector2D AxisEnd;
	const FVector WorldOrigin = InLocalToWorld.GetTranslation();
	const FVector WorldAxisEnd = WorldOrigin + InLocalToWorld.TransformVectorNoScale(InLocalAxis * 64.0);

	if (View->ScreenToPixel(View->WorldToScreen(WorldOrigin), Origin) &&
		View->ScreenToPixel(View->WorldToScreen(WorldAxisEnd), AxisEnd))
	{
		// If both the origin and the axis endpoint are in front of the camera, trivially calculate the viewport space axis direction
		return (AxisEnd - Origin).GetSafeNormal();
	}
	
	// If either the origin or axis endpoint are behind the camera, translate the entire widget in front of the camera in the view direction before performing the
	// viewport space calculation
	const FMatrix InvViewMatrix = View->ViewMatrices.GetInvViewMatrix();
	const FVector ViewLocation = InvViewMatrix.GetOrigin();
	const FVector ViewDirection = InvViewMatrix.GetUnitAxis(EAxis::Z);
	const FVector Offset = ViewDirection * (FVector::DotProduct(ViewLocation - WorldOrigin, ViewDirection) + 100.0f);
	const FVector AdjustedWidgetOrigin = WorldOrigin + Offset;
	const FVector AdjustedWidgetAxisEnd = WorldAxisEnd + Offset;

	View->ScreenToPixel(View->WorldToScreen(AdjustedWidgetOrigin), Origin);
	View->ScreenToPixel(View->WorldToScreen(AdjustedWidgetAxisEnd), AxisEnd);
	return -(AxisEnd - Origin).GetSafeNormal();	
}

void UTransformGizmo::ApplyTranslateDelta(const FVector& InTranslateDelta)
{
	CurrentTransform.AddToTranslation(InTranslateDelta);
	ActiveTarget->SetTransform(CurrentTransform);
}

void UTransformGizmo::ApplyRotateDelta(const FQuat& InRotateDelta)
{
	// Applies rot delta after the current rotation
	FQuat NewRotation = InRotateDelta * CurrentTransform.GetRotation();
	CurrentTransform.SetRotation(NewRotation);
	ActiveTarget->SetTransform(CurrentTransform);
}

void UTransformGizmo::ApplyScaleDelta(const FVector& InScaleDelta)
{
	FVector StartScale = CurrentTransform.GetScale3D();
	FVector NewScale = StartScale + InScaleDelta;
	CurrentTransform.SetScale3D(NewScale);
	ActiveTarget->SetTransform(CurrentTransform);
}

#undef LOCTEXT_NAMESPACE
