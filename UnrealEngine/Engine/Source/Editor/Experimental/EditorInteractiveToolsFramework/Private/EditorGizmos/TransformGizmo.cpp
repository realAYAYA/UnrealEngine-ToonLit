// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/TransformGizmo.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseGizmos/AxisSources.h"
#include "BaseGizmos/GizmoElementGroup.h"
#include "BaseGizmos/GizmoElementShapes.h"
#include "BaseGizmos/GizmoMath.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/StateTargets.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "UnrealEngine.h"
#include "Behaviors/MultiButtonClickDragBehavior.h"
#include "Intersection/IntersectionUtil.h"
#include "SceneManagement.h"

#define LOCTEXT_NAMESPACE "UTransformGizmo"

DEFINE_LOG_CATEGORY_STATIC(LogTransformGizmo, Log, All);

namespace GizmoLocals
{
	
// NOTE these variables are not intended to remain here indefinitely.
// Their purpose is to experiment the new behavior of rotation gizmos.

static float DotThreshold = 0.2f;
static FAutoConsoleVariableRef CVarDotThreshold(
	TEXT("Gizmos.DotThreshold"),
	DotThreshold,
	TEXT("Dot threshold for determining whether the rotation plane is perpendicular to the camera view [0.2, 1.0]"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
	{
		DotThreshold = FMath::Clamp(DotThreshold, 0.2, 1.0);
	})
);

static bool	bDebugDraw = false;
static FAutoConsoleVariableRef CVarDebugDraw(
	TEXT("Gizmos.DebugDraw"),
	bDebugDraw,
	TEXT("Displays debugging information.")
	);

static bool	ProjectIndirect = true;
static FAutoConsoleVariableRef CVarProjectIndirect(
	TEXT("Gizmos.ProjectIndirect"),
	ProjectIndirect,
	TEXT("Project to the nearest point of the curve when handling indirect rotation.")
	);
}

void UTransformGizmo::SetDisallowNegativeScaling(bool bDisallow)
{
	bDisallowNegativeScaling = bDisallow;
}

void UTransformGizmo::Setup()
{
	if (IsValid(GizmoElementRoot))
	{
		return;
	}
	
	UInteractiveGizmo::Setup();

	SetupBehaviors();
	SetupIndirectBehaviors();
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

	SetModeLastHitPart(EGizmoTransformMode::None, ETransformGizmoPartIdentifier::Default);
	SetModeLastHitPart(EGizmoTransformMode::Translate, ETransformGizmoPartIdentifier::TranslateScreenSpace);
	SetModeLastHitPart(EGizmoTransformMode::Rotate, ETransformGizmoPartIdentifier::RotateArcball);
	SetModeLastHitPart(EGizmoTransformMode::Scale, ETransformGizmoPartIdentifier::ScaleUniform);
}

void UTransformGizmo::SetupBehaviors()
{
	// Add default mouse hover behavior
	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	HoverBehavior->Initialize(this);
	HoverBehavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY));
	AddInputBehavior(HoverBehavior);

	// Add default mouse input behavior
	UClickDragInputBehavior* MouseBehavior = NewObject<UClickDragInputBehavior>();
	MouseBehavior->Initialize(this);
	MouseBehavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY));
	AddInputBehavior(MouseBehavior);
}

void UTransformGizmo::SetupIndirectBehaviors()
{
	// Add middle mouse input behavior for indirect manipulation
	ULocalClickDragInputBehavior* MiddleClickDragBehavior = NewObject<ULocalClickDragInputBehavior>();
	MiddleClickDragBehavior->Initialize();
	MiddleClickDragBehavior->SetUseMiddleMouseButton();
	MiddleClickDragBehavior->CanBeginClickDragFunc = [this](const FInputDeviceRay&)
	{
		static const FInputRayHit InvalidRayHit;
		static const FInputRayHit ValidRayHit(TNumericLimits<double>::Max());
		return CanInteract() ? ValidRayHit : InvalidRayHit;
	};
	MiddleClickDragBehavior->OnClickPressFunc = [this](const FInputDeviceRay& InPressPos)
	{
		bIndirectManipulation = true;
		if (LastHitPart == ETransformGizmoPartIdentifier::Default)
		{
			LastHitPart = GetCurrentModeLastHitPart();
		}
		return OnClickPress(InPressPos);
	};
	MiddleClickDragBehavior->OnClickDragFunc = [this](const FInputDeviceRay& InDragPos)
	{
		bIndirectManipulation = true;
		return OnClickDrag(InDragPos);
	};
	MiddleClickDragBehavior->OnClickReleaseFunc = [this](const FInputDeviceRay& InReleasePos)
	{
		bIndirectManipulation = false;
		return OnClickRelease(InReleasePos);
	};
	MiddleClickDragBehavior->OnTerminateFunc = [this]()
	{
		bIndirectManipulation = false;
		return OnTerminateDragSequence();
	};
	// disable ctrl + mmb for that behavior?
	MiddleClickDragBehavior->ModifierCheckFunc = [this](const FInputDeviceState& InputState)
	{
		return !bCtrlMiddleDoesY || !FInputDeviceState::IsCtrlKeyDown(InputState);
	};
	AddInputBehavior(MiddleClickDragBehavior);

	// Add left/right mouse input behavior for indirect manipulation
	MultiIndirectClickDragBehavior = NewObject<UMultiButtonClickDragBehavior>();
	MultiIndirectClickDragBehavior->Initialize();
	MultiIndirectClickDragBehavior->EnableButton(EKeys::LeftMouseButton);
	if (bCtrlMiddleDoesY)
	{
		MultiIndirectClickDragBehavior->EnableButton(EKeys::MiddleMouseButton);
	}
	MultiIndirectClickDragBehavior->EnableButton(EKeys::RightMouseButton);
	MultiIndirectClickDragBehavior->ModifierCheckFunc = FInputDeviceState::IsCtrlKeyDown;
	MultiIndirectClickDragBehavior->CanBeginClickDragFunc = [this](const FInputDeviceRay&)
	{
		static const FInputRayHit InvalidRayHit;
		static const FInputRayHit ValidRayHit(TNumericLimits<double>::Max());
		return CanInteract() ? ValidRayHit : InvalidRayHit;
	};
	MultiIndirectClickDragBehavior->OnClickPressFunc = [this](const FInputDeviceRay& InPressPos)
	{
		bIndirectManipulation = true;
		if (LastHitPart == ETransformGizmoPartIdentifier::Default)
		{
			LastHitPart = GetCurrentModeLastHitPart();
		}
		return OnClickPress(InPressPos);
	};
	MultiIndirectClickDragBehavior->OnClickDragFunc = [this](const FInputDeviceRay& InDragPos)
	{
		bIndirectManipulation = true;
		return OnClickDrag(InDragPos);
	};
	MultiIndirectClickDragBehavior->OnClickReleaseFunc = [this](const FInputDeviceRay& InReleasePos)
	{
		bIndirectManipulation = false;
		return OnClickRelease(InReleasePos);
	};
	MultiIndirectClickDragBehavior->OnTerminateFunc = [this]()
	{
		bIndirectManipulation = false;
		return OnTerminateDragSequence();
	};
	
	auto GetAxis = [this](const FInputDeviceState& InInput)
	{
		const bool bAddX = InInput.Mouse.Left.bDown;
		if (bCtrlMiddleDoesY)
		{
			const bool bAddY = InInput.Mouse.Middle.bDown;
			const bool bAddZ = InInput.Mouse.Right.bDown;
			return bAddX ? EAxis::X : bAddY ? EAxis::Y : bAddZ ? EAxis::Z : EAxis::None;
		}
		const bool bAddY = InInput.Mouse.Right.bDown;
		return bAddX && bAddY ? EAxis::Z : bAddX ? EAxis::X : bAddY ? EAxis::Y : EAxis::None;
	};

	auto GetHitPart = [this](const EAxis::Type InAxis)
	{
		static constexpr ETransformGizmoPartIdentifier TranslateIds [4] = {
			ETransformGizmoPartIdentifier::Default,
			ETransformGizmoPartIdentifier::TranslateXAxis,
			ETransformGizmoPartIdentifier::TranslateYAxis,
			ETransformGizmoPartIdentifier::TranslateZAxis};
			
		static constexpr ETransformGizmoPartIdentifier RotateIds [4] = {
			ETransformGizmoPartIdentifier::Default,
			ETransformGizmoPartIdentifier::RotateXAxis,
			ETransformGizmoPartIdentifier::RotateYAxis,
			ETransformGizmoPartIdentifier::RotateZAxis};

		static constexpr ETransformGizmoPartIdentifier ScaleIds [4] = {
			ETransformGizmoPartIdentifier::Default,
			ETransformGizmoPartIdentifier::ScaleXAxis,
			ETransformGizmoPartIdentifier::ScaleYAxis,
			ETransformGizmoPartIdentifier::ScaleZAxis};

		switch (CurrentMode)
		{
		case EGizmoTransformMode::Translate: return TranslateIds[InAxis];
		case EGizmoTransformMode::Rotate: return RotateIds[InAxis];
		case EGizmoTransformMode::Scale: return ScaleIds[InAxis];
		default: break;
		}
		return ETransformGizmoPartIdentifier::Default;
	};
	
	MultiIndirectClickDragBehavior->OnStateUpdated = [this, GetAxis, GetHitPart](const FInputDeviceState& Input)
	{
		// disable indirect if the current axis is none 
		const EAxis::Type Axis = GetAxis(Input);
		if (Axis == EAxis::None)
		{
			bIndirectManipulation = false;
			return;
		}

		bIndirectManipulation = true;
		
		const ETransformGizmoPartIdentifier HitPart = GetHitPart(Axis);
		if (HitPart != GetCurrentModeLastHitPart())
		{
			// update interaction state
			UpdateInteractingState(false, GetCurrentModeLastHitPart(), true);
			SetModeLastHitPart(CurrentMode, HitPart);
			UpdateInteractingState(true, HitPart, true);

			// reinitialize OnClickPress data
			LastHitPart = HitPart;
			const uint8 HitPartIndex = static_cast<uint8>(LastHitPart);
			if (OnClickPressFunctions.IsValidIndex(HitPartIndex) && OnClickPressFunctions[HitPartIndex])
			{
				OnClickPressFunctions[HitPartIndex](this, MultiIndirectClickDragBehavior->GetDeviceRay(Input));
			}
		}
	};
	AddInputBehavior(MultiIndirectClickDragBehavior);
}

void UTransformGizmo::SetupMaterials()
{
	auto GetBaseMaterial = [this]() -> UMaterial*
	{
		if (CustomizationFunction)
		{
			const FGizmoCustomization& GizmoCustomization = CustomizationFunction();
			if (IsValid(GizmoCustomization.Material))
			{
				return GizmoCustomization.Material.Get();
			}
		}

		static const FString MaterialName = TEXT("/Engine/EditorMaterials/TransformGizmoMaterial.TransformGizmoMaterial");
		UMaterial* Material = FindObject<UMaterial>(nullptr, *MaterialName);
		if (!Material)
		{
			Material = LoadObject<UMaterial>(nullptr, *MaterialName, nullptr, LOAD_None, nullptr);
		}
		
		return Material ? Material : GEngine->ArrowMaterial.Get();
	};
	
	UMaterial* AxisMaterialBase = GetBaseMaterial(); 

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
	OnSetActiveTarget.Clear();
	OnAboutToClearActiveTarget.Clear();
}

FTransform UTransformGizmo::GetGizmoTransform() const
{
	const float Scale = TransformGizmoSource ? TransformGizmoSource->GetGizmoScale() : 1.0f;
	
	auto GetCoordinateSystem = [&]()
	{
		if (TransformGizmoSource)
		{
			return TransformGizmoSource->GetGizmoCoordSystemSpace(); 
		}
		return GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	};
	const bool bLocal = GetCoordinateSystem() == EToolContextCoordinateSystem::Local;
	
	FTransform GizmoLocalToWorldTransform(CurrentTransform.GetTranslation());
	if (bLocal)
	{
		GizmoLocalToWorldTransform.SetRotation(CurrentTransform.GetRotation());
	}
	GizmoLocalToWorldTransform.SetScale3D(FVector(Scale, Scale, Scale));

	return GizmoLocalToWorldTransform;
}

void UTransformGizmo::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (IsVisible() && GizmoElementRoot && RenderAPI)
	{
		CurrentTransform = ActiveTarget->GetTransform();

		UGizmoElementBase::FRenderTraversalState RenderState;
		RenderState.Initialize(RenderAPI->GetSceneView(), GetGizmoTransform());
		GizmoElementRoot->Render(RenderAPI, RenderState);

		if (GizmoLocals::bDebugDraw)
		{
			if (bDebugRotate)
			{
				const float Radius = 2.f * GetWorldRadius(RotateAxisOuterRadius);
				FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
				PDI->DrawLine(DebugClosest - (DebugDirection * Radius), DebugClosest + (DebugDirection * Radius), FLinearColor::Yellow, SDPG_Foreground);
			}
		}
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
	const FInputRayHit RayHit = UpdateHoveredPart(DevicePos);
	return RayHit.bHit;
}

void UTransformGizmo::OnEndHover()
{
	if (HitTarget)
	{
		if (LastHitPart != ETransformGizmoPartIdentifier::Default)
		{
			UpdateHoverState(false, LastHitPart);
		}

		const ETransformGizmoPartIdentifier ModeHitPart = GetCurrentModeLastHitPart();
		if (ModeHitPart != ETransformGizmoPartIdentifier::Default)
		{
			UpdateInteractingState(true, ModeHitPart, true);
		}
	}
}

FInputRayHit UTransformGizmo::UpdateHoveredPart(const FInputDeviceRay& PressPos)
{
	if (!HitTarget || !IsVisible())
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

	const ETransformGizmoPartIdentifier ModeHitPart = GetCurrentModeLastHitPart();
	if (ModeHitPart != ETransformGizmoPartIdentifier::Default)
	{
		UpdateInteractingState(true, ModeHitPart, true);
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

void UTransformGizmo::SetModeLastHitPart(const EGizmoTransformMode InMode, const ETransformGizmoPartIdentifier InIdentifier)
{
	if (InMode >= EGizmoTransformMode::None && InMode < EGizmoTransformMode::Max)
	{
		LastHitPartPerMode[static_cast<uint8>(InMode)] = InIdentifier;
	}
}

ETransformGizmoPartIdentifier UTransformGizmo::GetCurrentModeLastHitPart() const
{
	auto GetTransformMode = [&]()
	{
		if (TransformGizmoSource)
		{
			return TransformGizmoSource->GetGizmoMode();
		}
		
		const EToolContextTransformGizmoMode ActiveGizmoMode = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentTransformGizmoMode();
		switch (ActiveGizmoMode)
		{
		case EToolContextTransformGizmoMode::NoGizmo:
			return EGizmoTransformMode::None;
		case EToolContextTransformGizmoMode::Translation:
			return EGizmoTransformMode::Translate;
		case EToolContextTransformGizmoMode::Rotation:
			return EGizmoTransformMode::Rotate;
		case EToolContextTransformGizmoMode::Scale:
			return EGizmoTransformMode::Scale;
		case EToolContextTransformGizmoMode::Combined:
			return EGizmoTransformMode::None;
		}
		
		return EGizmoTransformMode::None;
	};

	const EGizmoTransformMode GizmoTransformMode = GetTransformMode();
	return (GizmoTransformMode < EGizmoTransformMode::Max) ?
		LastHitPartPerMode[static_cast<uint8>(GizmoTransformMode)] :
		ETransformGizmoPartIdentifier::Default; 
}

FInputRayHit UTransformGizmo::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FInputRayHit RayHit;
		
	if (CanInteract() && HitTarget)
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
	auto GetTransformMode = [&]()
	{
		if (TransformGizmoSource)
		{
			return TransformGizmoSource->GetGizmoMode();
		}
		
		const EToolContextTransformGizmoMode ActiveGizmoMode = GetGizmoManager()->GetContextQueriesAPI()->GetCurrentTransformGizmoMode();
		switch (ActiveGizmoMode)
		{
			case EToolContextTransformGizmoMode::Translation: return EGizmoTransformMode::Translate;
			case EToolContextTransformGizmoMode::Rotation: return EGizmoTransformMode::Rotate;
			case EToolContextTransformGizmoMode::Scale: return EGizmoTransformMode::Scale;
		}
		return EGizmoTransformMode::None;
	};

	auto GetAxisToDraw = [&]()
	{
		if (TransformGizmoSource)
		{
			return TransformGizmoSource->GetGizmoAxisToDraw(TransformGizmoSource->GetGizmoMode());
		}
		return EAxisList::Type::All;
	};

	const EGizmoTransformMode NewMode = GetTransformMode();
	const EAxisList::Type NewAxisToDraw = GetAxisToDraw();
	
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

		if (RotateArcballElement == nullptr)
		{
			RotateArcballElement = MakeRotateCircleHandle(ETransformGizmoPartIdentifier::RotateArcball, RotateArcballSphereRadius, RotateArcballCircleColor, true);
			GizmoElementRoot->Add(RotateArcballElement);
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

	if (RotateArcballElement)
	{ 
		RotateArcballElement->SetEnabled(bEnableAll);
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

bool UTransformGizmo::IsVisible() const
{
	if (TransformGizmoSource)
	{
		return bVisible && TransformGizmoSource->GetVisible();
	}	
	return bVisible;
}

bool UTransformGizmo::CanInteract() const
{
	const bool bValidMode = CurrentMode > EGizmoTransformMode::None && CurrentMode < EGizmoTransformMode::Max;
	if (TransformGizmoSource)
	{
		return bValidMode && TransformGizmoSource->CanInteract(); 
	}
	return bValidMode && bVisible;
}

void UTransformGizmo::Tick(float DeltaTime)
{
	if (PendingDragFunction)
	{
		PendingDragFunction();
		PendingDragFunction.Reset();
	}
	
	UpdateMode();

	UpdateCameraAxisSource();
}

void UTransformGizmo::SetActiveTarget(UTransformProxy* Target, IToolContextTransactionProvider* TransactionProvider, IGizmoStateTarget* InStateTarget)
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

	if (InStateTarget)
	{
		StateTarget.SetInterface(InStateTarget);
		StateTarget.SetObject(CastChecked<UObject>(InStateTarget));
	}
	else
	{
		StateTarget = UGizmoObjectModifyStateTarget::Construct(Target,
			LOCTEXT("UTransformGizmoTransaction", "Transform"), TransactionProvider, this);	
	}

	CameraAxisSource = NewObject<UGizmoConstantFrameAxisSource>(this);

	OnSetActiveTarget.Broadcast(this, ActiveTarget);
}

// @todo: This should either be named to "SetScale" or removed, since it can be done with ReinitializeGizmoTransform
void UTransformGizmo::SetNewChildScale(const FVector& NewChildScale)
{
	FTransform NewTransform = ActiveTarget->GetTransform();
	NewTransform.SetScale3D(NewChildScale);

	TGuardValue GuardValue(ActiveTarget->bSetPivotMode, true);
	ActiveTarget->SetTransform(NewTransform);
}

void UTransformGizmo::SetVisibility(bool bVisibleIn)
{
	bVisible = bVisibleIn;
}

void UTransformGizmo::SetCustomizationFunction(const TFunction<const FGizmoCustomization()>& InFunction)
{
	CustomizationFunction = InFunction;
}

void UTransformGizmo::HandleWidgetModeChanged(UE::Widget::EWidgetMode InWidgetMode)
{
	auto GetTransformMode = [InWidgetMode]()
	{
		switch (InWidgetMode)
		{
		case UE::Widget::EWidgetMode::WM_Translate: return EGizmoTransformMode::Translate;
		case UE::Widget::EWidgetMode::WM_Rotate: return EGizmoTransformMode::Rotate;
		case UE::Widget::EWidgetMode::WM_Scale: return EGizmoTransformMode::Scale;
		default: return EGizmoTransformMode::None;
		}
	};
	const EGizmoTransformMode NewMode = GetTransformMode();

	if (CurrentMode != EGizmoTransformMode::None && NewMode == CurrentMode)
	{
		const ETransformGizmoPartIdentifier CurrentModeLastHitPart = GetCurrentModeLastHitPart();
		auto GetModeDefaultHitPart = [NewMode, CurrentModeLastHitPart]()
		{
			const bool bIsRotateArcBall = (CurrentModeLastHitPart == ETransformGizmoPartIdentifier::RotateArcball);
			switch (NewMode)
			{
			case EGizmoTransformMode::Translate:
				return ETransformGizmoPartIdentifier::TranslateScreenSpace;
			case EGizmoTransformMode::Rotate:
				return bIsRotateArcBall ? ETransformGizmoPartIdentifier::RotateScreenSpace : ETransformGizmoPartIdentifier::RotateArcball;
			case EGizmoTransformMode::Scale:
				return ETransformGizmoPartIdentifier::ScaleUniform;
			default:
				return ETransformGizmoPartIdentifier::Default;
			}
		};

		const ETransformGizmoPartIdentifier DefaultHitPart = GetModeDefaultHitPart();
		if (DefaultHitPart != CurrentModeLastHitPart)
		{
			// reset indirect manipulation to default
			ResetInteractingStates(CurrentMode);
			ResetHoverStates(CurrentMode);
			
			SetModeLastHitPart(CurrentMode, DefaultHitPart);
			UpdateInteractingState(true, DefaultHitPart, true);
		}
	}
}

void UTransformGizmo::OnParametersChanged(const FGizmosParameters& InParameters)
{
	if (InParameters.bCtrlMiddleDoesY != bCtrlMiddleDoesY)
	{
		bCtrlMiddleDoesY = InParameters.bCtrlMiddleDoesY;

		// update CTRL + LMB/MMB/RMB indirect behavior
		if (MultiIndirectClickDragBehavior)
		{
			if (bCtrlMiddleDoesY)
			{
				MultiIndirectClickDragBehavior->EnableButton(EKeys::MiddleMouseButton);
			}
			else
			{
				MultiIndirectClickDragBehavior->DisableButton(EKeys::MiddleMouseButton);
			}
		}
	}
	
	DefaultRotateMode = InParameters.RotateMode;
}

UGizmoElementArrow* UTransformGizmo::MakeTranslateAxis(ETransformGizmoPartIdentifier InPartId, const FVector& InAxisDir, const FVector& InSideDir, UMaterialInterface* InMaterial)
{
	const float SizeCoeff = GetSizeCoefficient();
	
	UGizmoElementArrow* ArrowElement = NewObject<UGizmoElementArrow>();
	ArrowElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	ArrowElement->SetHeadType(EGizmoElementArrowHeadType::Cone);
	ArrowElement->SetBase(InAxisDir * AxisLengthOffset);
	ArrowElement->SetDirection(InAxisDir);
	ArrowElement->SetSideDirection(InSideDir);
	ArrowElement->SetBodyLength(TranslateAxisLength * SizeCoeff);
	ArrowElement->SetBodyRadius(AxisRadius);
	ArrowElement->SetHeadLength(TranslateAxisConeHeight * SizeCoeff);
	ArrowElement->SetHeadRadius(TranslateAxisConeRadius * SizeCoeff);
	ArrowElement->SetNumSides(32);
	ArrowElement->SetMaterial(InMaterial);
	ArrowElement->SetViewDependentType(EGizmoElementViewDependentType::Axis);
	ArrowElement->SetViewDependentAxis(InAxisDir);
	return ArrowElement;
}

UGizmoElementArrow* UTransformGizmo::MakeScaleAxis(ETransformGizmoPartIdentifier InPartId, const FVector& InAxisDir, const FVector& InSideDir, UMaterialInterface* InMaterial)
{
	const float SizeCoeff = GetSizeCoefficient();

	UGizmoElementArrow* ArrowElement = NewObject<UGizmoElementArrow>();
	ArrowElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	ArrowElement->SetHeadType(EGizmoElementArrowHeadType::Cube);
	ArrowElement->SetBase(InAxisDir * AxisLengthOffset);
	ArrowElement->SetDirection(InAxisDir);
	ArrowElement->SetSideDirection(InSideDir);
	ArrowElement->SetBodyLength(ScaleAxisLength * SizeCoeff);
	ArrowElement->SetBodyRadius(AxisRadius);
	ArrowElement->SetHeadLength(ScaleAxisCubeDim * SizeCoeff);
	ArrowElement->SetNumSides(32);
	ArrowElement->SetMaterial(InMaterial);
	ArrowElement->SetViewDependentType(EGizmoElementViewDependentType::Axis);
	ArrowElement->SetViewDependentAxis(InAxisDir);
	return ArrowElement;
}

UGizmoElementBox* UTransformGizmo::MakeUniformScaleHandle()
{
	const float SizeCoeff = GetSizeCoefficient();
	
	UGizmoElementBox* BoxElement = NewObject<UGizmoElementBox>();
	BoxElement->SetPartIdentifier(static_cast<uint32>(ETransformGizmoPartIdentifier::ScaleUniform));
	BoxElement->SetCenter(FVector::ZeroVector);
	BoxElement->SetUpDirection(FVector::UpVector);
	BoxElement->SetSideDirection(FVector::RightVector);
	BoxElement->SetDimensions(FVector(ScaleAxisCubeDim, ScaleAxisCubeDim, ScaleAxisCubeDim) * SizeCoeff);
	BoxElement->SetMaterial(GreyMaterial);
	return BoxElement;
}

UGizmoElementRectangle* UTransformGizmo::MakePlanarHandle(ETransformGizmoPartIdentifier InPartId, const FVector& InUpDirection, const FVector& InSideDirection, const FVector& InPlaneNormal,
	UMaterialInterface* InMaterial, const FLinearColor& InVertexColor)
{
	const float SizeCoeff = GetSizeCoefficient();
	
	FVector PlanarHandleCenter = (InUpDirection + InSideDirection) * PlanarHandleOffset * SizeCoeff;

	FLinearColor LineColor = InVertexColor;
	FLinearColor VertexColor = LineColor;
	VertexColor.A = LargeOuterAlpha;

	UGizmoElementRectangle* RectangleElement = NewObject<UGizmoElementRectangle>();
	RectangleElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	RectangleElement->SetUpDirection(InUpDirection);
	RectangleElement->SetSideDirection(InSideDirection);
	RectangleElement->SetCenter(PlanarHandleCenter);
	RectangleElement->SetHeight(PlanarHandleSize * SizeCoeff);
	RectangleElement->SetWidth(PlanarHandleSize * SizeCoeff);
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
	const float SizeCoeff = GetSizeCoefficient();
	
	UGizmoElementRectangle* RectangleElement = NewObject<UGizmoElementRectangle>();
	RectangleElement->SetPartIdentifier(static_cast<uint32>(ETransformGizmoPartIdentifier::TranslateScreenSpace));
	RectangleElement->SetUpDirection(FVector::UpVector);
	RectangleElement->SetSideDirection(FVector::RightVector);
	RectangleElement->SetCenter(FVector::ZeroVector);
	RectangleElement->SetHeight(TranslateScreenSpaceHandleSize * SizeCoeff);
	RectangleElement->SetWidth(TranslateScreenSpaceHandleSize * SizeCoeff);
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
	const float SizeCoeff = GetSizeCoefficient();
	
	UGizmoElementTorus* RotateAxisElement = NewObject<UGizmoElementTorus>();
	RotateAxisElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	RotateAxisElement->SetCenter(FVector::ZeroVector);
	RotateAxisElement->SetRadius(UTransformGizmo::RotateAxisOuterRadius * SizeCoeff);
	RotateAxisElement->SetNumSegments(UTransformGizmo::RotateAxisNumSegments);
	RotateAxisElement->SetInnerRadius(UTransformGizmo::RotateAxisInnerRadius * SizeCoeff);
	RotateAxisElement->SetNumInnerSlices(UTransformGizmo::RotateAxisInnerSlices);
	RotateAxisElement->SetAxis0(TorusAxis0);
	RotateAxisElement->SetAxis1(TorusAxis1);
	const FVector TorusNormal = RotateAxisElement->GetAxis0() ^ RotateAxisElement->GetAxis1();
	RotateAxisElement->SetPartialType(EGizmoElementPartialType::PartialViewDependent);
	RotateAxisElement->SetPartialStartAngle(0.0f);
	RotateAxisElement->SetPartialEndAngle(UE_PI);
	RotateAxisElement->SetViewDependentAxis(TorusNormal);
	RotateAxisElement->SetViewAlignType(EGizmoElementViewAlignType::Axial);
	RotateAxisElement->SetViewAlignAxialAngleTol(UE_DOUBLE_SMALL_NUMBER);
	RotateAxisElement->SetPartialViewDependentMaxCosTol(1.0-UE_DOUBLE_SMALL_NUMBER);	
	RotateAxisElement->SetViewAlignAxis(TorusNormal);
	RotateAxisElement->SetViewAlignNormal(TorusAxis1);
	RotateAxisElement->SetMaterial(InMaterial);
	return RotateAxisElement;
}

UGizmoElementCircle* UTransformGizmo::MakeRotateCircleHandle(ETransformGizmoPartIdentifier InPartId, float InRadius, const FLinearColor& InColor, float bFill)
{
	const float SizeCoeff = GetSizeCoefficient();
	
	UGizmoElementCircle* CircleElement = NewObject<UGizmoElementCircle>();
	CircleElement->SetPartIdentifier(static_cast<uint32>(InPartId));
	CircleElement->SetCenter(FVector::ZeroVector);
	CircleElement->SetRadius(InRadius * SizeCoeff);
	CircleElement->SetAxis0(FVector::UpVector);
	CircleElement->SetAxis1(-FVector::RightVector);
	CircleElement->SetLineColor(InColor);
	CircleElement->SetViewAlignType(EGizmoElementViewAlignType::PointOnly);
	CircleElement->SetViewAlignNormal(-FVector::ForwardVector);

	if (bFill)
	{
		const FLinearColor LightColor(InColor.R, InColor.G, InColor.B, InColor.A * 0.5);
		CircleElement->SetVertexColor(LightColor);
		CircleElement->SetMaterial(TransparentVertexColorMaterial);

		CircleElement->SetHoverVertexColor(InColor);
		CircleElement->SetHoverMaterial(TransparentVertexColorMaterial);
		
		CircleElement->SetInteractVertexColor(InColor);
		CircleElement->SetInteractMaterial(TransparentVertexColorMaterial);
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

	if (ActiveTarget)
	{
		OnAboutToClearActiveTarget.Broadcast(this, ActiveTarget);
		
		ActiveTarget->OnBeginTransformEdit.RemoveAll(this);
		ActiveTarget->OnEndTransformEdit.RemoveAll(this);
		ActiveTarget = nullptr;
	}
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
	auto GetCoordinateSystem = [&]()
	{
		if (TransformGizmoSource)
		{
			return TransformGizmoSource->GetGizmoCoordSystemSpace(); 
		}
		return GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	};
	
	if (GetCoordinateSystem() == EToolContextCoordinateSystem::Local)
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
	OnClickPressFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateArcball)] = &UTransformGizmo::OnClickPressArcBallRotate;

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
	OnClickDragFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateArcball)] = &UTransformGizmo::OnClickDragArcBallRotate;

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
	OnClickReleaseFunctions[static_cast<int>(ETransformGizmoPartIdentifier::RotateArcball)] = &UTransformGizmo::OnClickReleaseArcBallRotate;
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

void UTransformGizmo::UpdateHoverState(const bool bInHover, const ETransformGizmoPartIdentifier InHitPartId)
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
	default:
		break;
	}
}

void UTransformGizmo::ResetHoverStates(const EGizmoTransformMode InMode)
{
	ETransformGizmoPartIdentifier IdBegin = ETransformGizmoPartIdentifier::Default;
	ETransformGizmoPartIdentifier IdEnd = ETransformGizmoPartIdentifier::Max;

	switch (InMode)
	{
	case EGizmoTransformMode::Translate:
		IdBegin = ETransformGizmoPartIdentifier::TranslateAll;
		IdEnd = ETransformGizmoPartIdentifier::RotateAll;
		break;
	case EGizmoTransformMode::Rotate:
		IdBegin = ETransformGizmoPartIdentifier::RotateAll;
		IdEnd = ETransformGizmoPartIdentifier::ScaleAll;
		break;
	case EGizmoTransformMode::Scale:
		IdBegin = ETransformGizmoPartIdentifier::ScaleAll;
		IdEnd = ETransformGizmoPartIdentifier::Max;
		break;
	default:
		break;
	}

	static constexpr bool bInHover = false;
	for (uint32 Id = static_cast<uint32>(IdBegin); Id < static_cast<uint32>(IdEnd); ++Id)
	{
		UpdateHoverState(bInHover, static_cast<ETransformGizmoPartIdentifier>(Id));
	}
}

void UTransformGizmo::UpdateInteractingState(const bool bInInteracting, const ETransformGizmoPartIdentifier InHitPartId, const bool bIdOnly)
{
	HitTarget->UpdateInteractingState(bInInteracting, static_cast<uint32>(InHitPartId));

	if (!bIdOnly)
	{
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
		default:
			break;
		}
	}
}

void UTransformGizmo::ResetInteractingStates(const EGizmoTransformMode InMode)
{
	ETransformGizmoPartIdentifier IdBegin = ETransformGizmoPartIdentifier::Default;
	ETransformGizmoPartIdentifier IdEnd = ETransformGizmoPartIdentifier::Max;
	bool bIdOnly = true;

	switch (InMode)
	{
	case EGizmoTransformMode::Translate:
		IdBegin = ETransformGizmoPartIdentifier::TranslateAll;
		IdEnd = ETransformGizmoPartIdentifier::RotateAll;
		break;
	case EGizmoTransformMode::Rotate:
		IdBegin = ETransformGizmoPartIdentifier::RotateAll;
		IdEnd = ETransformGizmoPartIdentifier::ScaleAll;
		break;
	case EGizmoTransformMode::Scale:
		IdBegin = ETransformGizmoPartIdentifier::ScaleAll;
		IdEnd = ETransformGizmoPartIdentifier::Max;
		bIdOnly = false; 
		break;
	default:
		break;
	}

	static constexpr bool bInInteracting = false;
	for (uint32 Id = static_cast<uint32>(IdBegin); Id < static_cast<uint32>(IdEnd); ++Id)
	{
		UpdateInteractingState(bInInteracting, static_cast<ETransformGizmoPartIdentifier>(Id), bIdOnly);
	}
}

void UTransformGizmo::BeginTransformEditSequence()
{
	if (ensure(StateTarget))
	{
		StateTarget->BeginUpdate();
	}

	if (ensure(ActiveTarget))
	{
		if (ActiveTarget->bSetPivotMode)
		{
			ActiveTarget->BeginPivotEditSequence();
		}
		else
		{
			ActiveTarget->BeginTransformEditSequence();
		}
	}
}

void UTransformGizmo::EndTransformEditSequence()
{
	if (ensure(StateTarget))
	{
		StateTarget->EndUpdate();
	}

	if (ensure(ActiveTarget))
	{
		if (ActiveTarget->bSetPivotMode)
		{
			ActiveTarget->EndPivotEditSequence();
		}
		else
		{
			ActiveTarget->EndTransformEditSequence();
		}
	}
}

void UTransformGizmo::OnClickPress(const FInputDeviceRay& PressPos)
{
	PendingDragFunction.Reset();
	
	check(OnClickPressFunctions.Num() == static_cast<int>(ETransformGizmoPartIdentifier::Max));

	const ETransformGizmoPartIdentifier ModeLastHitPart = GetCurrentModeLastHitPart();
	
	if (OnClickPressFunctions[static_cast<int>(LastHitPart)])
	{
		OnClickPressFunctions[static_cast<int>(LastHitPart)](this, PressPos);
	}

	if (bInInteraction)
	{
		if (HitTarget && LastHitPart != ETransformGizmoPartIdentifier::Default)
		{
			if (LastHitPart != ModeLastHitPart)
			{
				UpdateInteractingState(false, ModeLastHitPart, true);	
			}
			UpdateInteractingState(true, LastHitPart);
		}

		BeginTransformEditSequence();
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
		if (bDeferDrag)
		{ // defer drag function on next tick
			PendingDragFunction = [this, DragPos, HitPartIndex]()
			{
				OnClickDragFunctions[HitPartIndex](this, DragPos);
			};
		}
		else
		{ // do drag function
			OnClickDragFunctions[HitPartIndex](this, DragPos);
		}
	}
}

void UTransformGizmo::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	if (!bInInteraction)
	{
		return;
	}

	const int HitPartIndex = static_cast<int>(LastHitPart);
	check(HitPartIndex < OnClickReleaseFunctions.Num());

	if (OnClickReleaseFunctions[HitPartIndex])
	{
		OnClickReleaseFunctions[HitPartIndex](this, ReleasePos);
	}

	EndTransformEditSequence();

	bInInteraction = false;

	if (HitTarget && LastHitPart != ETransformGizmoPartIdentifier::Default)
	{
		UpdateInteractingState(false, LastHitPart);
		UpdateInteractingState(true, GetCurrentModeLastHitPart(), true);
	}

	PendingDragFunction.Reset();
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
	InteractionPlanarOrigin = CurrentTransform.GetLocation();
	InteractionAxisStartParam = GetNearestRayParamToInteractionAxis(PressPos);
	InteractionAxisCurrParam = InteractionAxisStartParam;

	// indirect manipulation uses a 2D approach instead as there's no guaranty to intersect a plane 
	if (bIndirectManipulation)
	{
		InteractionScreenCurrPos = PressPos.ScreenPosition;
		StartRotation = CurrentRotation = ActiveTarget->GetTransform().GetRotation();
		bInInteraction = true;
		SetModeLastHitPart(EGizmoTransformMode::Translate, LastHitPart);
		return;
	}

	// compute plane and axis to mute
	const FVector XAxis = GetWorldAxis(FVector::XAxisVector);
	const FVector YAxis = GetWorldAxis(FVector::YAxisVector);
	const FVector ZAxis = GetWorldAxis(FVector::ZAxisVector);

	const FVector ViewDirection = GizmoViewContext->GetViewDirection();
	const double XDot = FMath::Abs(FVector::DotProduct(ViewDirection, XAxis));
	const double YDot = FMath::Abs(FVector::DotProduct(ViewDirection, YAxis));
	const double ZDot = FMath::Abs(FVector::DotProduct(ViewDirection, ZAxis));

	if (FVector::DotProduct(InteractionAxisDirection, XAxis) > 0.1)
	{
		InteractionPlanarNormal = (YDot > ZDot) ? YAxis : ZAxis;
		NormalToRemove = (YDot > ZDot) ? ZAxis : YAxis;
	}
	else if (FVector::DotProduct(InteractionAxisDirection, YAxis) > 0.1)
	{
		InteractionPlanarNormal = (XDot > ZDot) ? XAxis : ZAxis;
		NormalToRemove = (XDot > ZDot) ? ZAxis : XAxis;
	}
	else
	{
		InteractionPlanarNormal = (XDot > YDot) ? XAxis : YAxis;
		NormalToRemove = (XDot > YDot) ? YAxis : XAxis;
	}
	
	float HitDepth;
	if (GetRayParamIntersectionWithInteractionPlane(PressPos, HitDepth))
	{
		InteractionPlanarStartPoint = PressPos.WorldRay.Origin + PressPos.WorldRay.Direction * HitDepth;
		InteractionPlanarCurrPoint = InteractionPlanarStartPoint;
	}

	bInInteraction = true;
	SetModeLastHitPart(EGizmoTransformMode::Translate, LastHitPart);
}

void UTransformGizmo::OnClickDragTranslateAxis(const FInputDeviceRay& DragPos)
{
	// indirect manipulation uses a 2D projection approach instead of plane intersection
	if (bIndirectManipulation)
	{
		const FVector2D DragDir = DragPos.ScreenPosition - InteractionScreenCurrPos;

		const FVector2D XAxisDir = GetScreenProjectedAxis(GizmoViewContext, FVector::XAxisVector, CurrentTransform);
		const FVector2D YAxisDir = GetScreenProjectedAxis(GizmoViewContext, FVector::YAxisVector, CurrentTransform);
		const FVector2D ZAxisDir = GetScreenProjectedAxis(GizmoViewContext, FVector::ZAxisVector, CurrentTransform);
		
		FVector Delta((InteractionAxisList == EAxisList::X) ? FVector2D::DotProduct(XAxisDir, DragDir) : 0.0,
					  (InteractionAxisList == EAxisList::Y) ? FVector2D::DotProduct(YAxisDir, DragDir) : 0.0,
					  (InteractionAxisList == EAxisList::Z) ? FVector2D::DotProduct(ZAxisDir, DragDir) : 0.0);
		Delta = CurrentRotation * Delta;
		
		ApplyTranslateDelta(Delta);

		InteractionScreenCurrPos = DragPos.ScreenPosition;
		return;
	}

	float HitDepth;
	if (GetRayParamIntersectionWithInteractionPlane(DragPos, HitDepth))
	{
		FVector HitPoint = DragPos.WorldRay.Origin + DragPos.WorldRay.Direction * HitDepth;

		const FVector DeltaToStart = HitPoint-InteractionPlanarStartPoint;
		const FVector AxisToRemove = NormalToRemove * FVector::DotProduct(DeltaToStart, NormalToRemove); 
		
		HitPoint -= AxisToRemove;
	
		const FVector Delta = ComputePlanarTranslateDelta(InteractionPlanarCurrPoint, HitPoint);

		ApplyTranslateDelta(Delta);
		InteractionPlanarCurrPoint = HitPoint;
	}
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
		
		SetModeLastHitPart(EGizmoTransformMode::Translate, LastHitPart);
	}
}

void UTransformGizmo::OnClickDragTranslatePlanar(const FInputDeviceRay& DragPos)
{
	float HitDepth;
	if (GetRayParamIntersectionWithInteractionPlane(DragPos, HitDepth))
	{
		const FVector HitPoint = DragPos.WorldRay.Origin + DragPos.WorldRay.Direction * HitDepth;
		const FVector Delta = ComputePlanarTranslateDelta(InteractionPlanarCurrPoint, HitPoint);
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

	SetModeLastHitPart(EGizmoTransformMode::Scale, LastHitPart);
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

	auto GetScaleType = [&]()
	{
		if (TransformGizmoSource)
		{
			return TransformGizmoSource->GetScaleType();
		}
		return EGizmoTransformScaleType::Default;
	};
	
	if (GetScaleType() != EGizmoTransformScaleType::PercentageBased)
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

bool UTransformGizmo::OnClickPressRotateArc(
	const FInputDeviceRay& InPressPos, const FVector& InPlaneNormal,
	const FVector& InPlaneAxis1, const FVector& InPlaneAxis2)
{
	const FRay& Ray = InPressPos.WorldRay;

	// compute axis / view direction projection: is the rotation plane nearly perpendicular to the view plane?
	const FVector WorldOrigin = CurrentTransform.GetLocation();
	const FVector ViewDirection = GizmoViewContext->IsPerspectiveProjection() ?
		(WorldOrigin - GizmoViewContext->ViewLocation).GetSafeNormal() :
		GizmoViewContext->GetViewDirection();
	const bool bAxisPerpendicularToView = FMath::Abs(FVector::DotProduct(InPlaneNormal, ViewDirection)) < GizmoLocals::DotThreshold;
	const bool bRayPerpendicularToAxis = FMath::IsNearlyZero(FVector::DotProduct(InPlaneNormal, Ray.Direction));

	// can we project
	const bool bCanProject = bIndirectManipulation || bAxisPerpendicularToView || bRayPerpendicularToAxis;
	if (!bCanProject)
	{
		InteractionPlanarOrigin = CurrentTransform.GetLocation();
		InteractionPlanarNormal = InPlaneNormal;
		InteractionPlanarAxisX = InPlaneAxis1;
		InteractionPlanarAxisY = InPlaneAxis2;
	
		float HitDepth;
		if (GetRayParamIntersectionWithInteractionPlane(InPressPos, HitDepth))
		{
			const FVector HitPoint = InPressPos.WorldRay.Origin + InPressPos.WorldRay.Direction * HitDepth;
			InteractionStartAngle = GizmoMath::ComputeAngleInPlane(HitPoint,
				InteractionPlanarOrigin, InteractionPlanarNormal, InteractionPlanarAxisX, InteractionPlanarAxisY);
			InteractionCurrAngle = InteractionStartAngle;
			bInInteraction = true;
			return true;
		}
	}
	return false;
}

void UTransformGizmo::OnClickPressRotateAxis(const FInputDeviceRay& InPressPos)
{
	static const TArray RotateIDs({	ETransformGizmoPartIdentifier::RotateXAxis,
									ETransformGizmoPartIdentifier::RotateYAxis,
									ETransformGizmoPartIdentifier::RotateZAxis});

	const int32 RotateID = RotateIDs.IndexOfByKey(LastHitPart);
	if (!ensure(RotateID != INDEX_NONE))
	{
		bInInteraction = false;
		return;
	}

	static const TArray AxisList({EAxisList::X, EAxisList::Y, EAxisList::Z});

	bDebugRotate = true;
	
	// initialize pull data
	InteractionScreenAxisDirection = GetScreenRotateAxisDir(InPressPos);
	InteractionAxisList = AxisList[RotateID];
	InteractionScreenStartPos = InteractionScreenCurrPos = InPressPos.ScreenPosition;
	RotateMode = EAxisRotateMode::Pull;

	// initialize arc/mixed data
	const bool bRotatePull = bIndirectManipulation || DefaultRotateMode == EAxisRotateMode::Pull;
	if (!bRotatePull)
	{
		static const TArray RotateAxis({FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector});
		const FVector WorldPlaneNormal = GetWorldAxis(RotateAxis[RotateID]);
		const FVector WorldPlaneAxis1 = GetWorldAxis(RotateAxis[(RotateID+1) % 3]);
		const FVector WorldPlaneAxis2 = GetWorldAxis(RotateAxis[(RotateID+2) % 3]);
		
		const bool bCanRotateArc = OnClickPressRotateArc(InPressPos, WorldPlaneNormal, WorldPlaneAxis1, WorldPlaneAxis2);
		if (bCanRotateArc)
		{
			RotateMode = EAxisRotateMode::Arc;
		}
	}

	bInInteraction = true;
	SetModeLastHitPart(EGizmoTransformMode::Rotate, LastHitPart);
}

void UTransformGizmo::OnClickPressRotateXAxis(const FInputDeviceRay& InPressPos)
{
	OnClickPressRotateAxis(InPressPos);
}

void UTransformGizmo::OnClickPressRotateYAxis(const FInputDeviceRay& InPressPos)
{
	OnClickPressRotateAxis(InPressPos);
}

void UTransformGizmo::OnClickPressRotateZAxis(const FInputDeviceRay& InPressPos)
{
	OnClickPressRotateAxis(InPressPos);
}

FVector2D UTransformGizmo::GetScreenRotateAxisDir(const FInputDeviceRay& InPressPos)
{
	// NOTE that function is not intended to remain here indefinitely, its purpose is to debug closest point computation
	const auto PrintProjection = [this](const TCHAR* const InMessage)
	{
		if (bDebugRotate && GizmoLocals::bDebugDraw)
		{
			UE_LOG(LogTransformGizmo, Warning, TEXT("%s"), InMessage);
		}
	};

	static const TArray RotateIDs({	ETransformGizmoPartIdentifier::RotateXAxis,
									ETransformGizmoPartIdentifier::RotateYAxis,
									ETransformGizmoPartIdentifier::RotateZAxis});
	const int32 RotateID = RotateIDs.IndexOfByKey(LastHitPart);
	if (!ensure(RotateID != INDEX_NONE))
	{
		return FVector2D::ZeroVector;
	}
	
	const FRay& Ray = InPressPos.WorldRay;

	// store world origin and axis
	static const TArray RotateAxis({FVector::XAxisVector, FVector::YAxisVector, -FVector::ZAxisVector});
	const FVector WorldAxis = GetWorldAxis(RotateAxis[RotateID]);
	const FVector WorldOrigin = CurrentTransform.GetLocation();

	// compute axis / view direction projection: is the rotation plane nearly perpendicular to the view plane?
	const FVector ViewDirection = GizmoViewContext->IsPerspectiveProjection() ?
		(WorldOrigin - GizmoViewContext->ViewLocation).GetSafeNormal() :
		GizmoViewContext->GetViewDirection();
	const bool bAxisPerpendicularToView = FMath::Abs(FVector::DotProduct(WorldAxis, ViewDirection)) < GizmoLocals::DotThreshold;
	// compute axis / ray direction projection: is the ray direction parallel to the axis?
	const bool bRayPerpendicularToAxis = FMath::IsNearlyZero(FVector::DotProduct(WorldAxis, Ray.Direction));

	// compute closest point on the rotate handle
	const bool bUseRayForIndirect = bIndirectManipulation && !GizmoLocals::ProjectIndirect;
	const bool bUseRayOrigin = bUseRayForIndirect || bAxisPerpendicularToView || bRayPerpendicularToAxis;
	// compute the closest point from plane intersection if we can
	FVector QueryPoint = Ray.Origin;
	if (!bUseRayOrigin)
	{
		const FPlane Plane(WorldOrigin, WorldAxis);

		// if the projection is in front of the camera then use it
		const double HitDepth = FMath::RayPlaneIntersectionParam(Ray.Origin, Ray.Direction, Plane);
		if (HitDepth >= 0.0)
		{
			PrintProjection(TEXT("front"));
			QueryPoint = Ray.Origin + Ray.Direction * HitDepth;
		}
		else
		{
			PrintProjection(TEXT("behind"));
		}
	}
	else
	{
		PrintProjection(TEXT("ray origin"));
	}

	// compute nearest point
	const float Radius = GetWorldRadius(RotateAxisOuterRadius);
	FVector ClosestPointOnCircle;
	GizmoMath::ClosetPointOnCircle(QueryPoint, WorldOrigin, WorldAxis, Radius, ClosestPointOnCircle);

	// compute world directions
	const FVector ToClosestDirection = (ClosestPointOnCircle - WorldOrigin).GetSafeNormal();
	const FVector PullDirection = FVector::CrossProduct(ToClosestDirection, WorldAxis);

	// compute screen projections
	const FTransform ToClosest(ClosestPointOnCircle);
	const FVector2D PullProjection = GetScreenProjectedAxis(GizmoViewContext, PullDirection, ToClosest);
	const FVector2D AxisProjection = GetScreenProjectedAxis(GizmoViewContext, WorldAxis, ToClosest);
	const FVector2D ToClosestProjection = GetScreenProjectedAxis(GizmoViewContext, ToClosestDirection, ToClosest);

	// compute which projection to remove from drag
	const double DotAxis = FMath::Abs( FVector2D::DotProduct(PullProjection, AxisProjection) );
	const double DotClosest = FMath::Abs( FVector2D::DotProduct(PullProjection, ToClosestProjection) );
	NormalProjectionToRemove = (DotAxis < DotClosest) ? AxisProjection : ToClosestProjection;

	// debug
	{
		DebugDirection = PullDirection;
		DebugClosest = ClosestPointOnCircle;
	}
	
	return PullProjection;
}

void UTransformGizmo::OnClickDragRotateAxis(const FInputDeviceRay& DragPos)
{
	switch (RotateMode)
	{
	case EAxisRotateMode::Pull:
	{
		const FQuat DeltaRot = ComputeAxisRotateDelta(InteractionScreenCurrPos, DragPos.ScreenPosition);
		ApplyRotateDelta(DeltaRot);
		InteractionScreenCurrPos = DragPos.ScreenPosition;
		break;
	}
	case EAxisRotateMode::Arc:
	{
		float HitDepth;
		if (GetRayParamIntersectionWithInteractionPlane(DragPos, HitDepth))
		{
			const FVector HitPoint = DragPos.WorldRay.Origin + DragPos.WorldRay.Direction * HitDepth;
			const float HitAngle = GizmoMath::ComputeAngleInPlane(HitPoint,
				InteractionPlanarOrigin, InteractionPlanarNormal, InteractionPlanarAxisX, InteractionPlanarAxisY);

			const FQuat Delta = ComputeAngularRotateDelta(InteractionCurrAngle, HitAngle);
	
			ApplyRotateDelta(Delta);
			InteractionCurrAngle = HitAngle;
		}
		break;
	}
	default:
		ensure(false);
		break;
	}
}

FQuat UTransformGizmo::ComputeAxisRotateDelta(const FVector2D& InStartPos, const FVector2D& InEndPos)
{
	FVector2D DragDir = InEndPos - InStartPos;

	const FVector2D DragDirToRemove = NormalProjectionToRemove * FVector2D::DotProduct(DragDir, NormalProjectionToRemove);
	DragDir -= DragDirToRemove;

	const double Delta = FVector2D::DotProduct(InteractionScreenAxisDirection, DragDir) * 0.25;
	FRotator DeltaRot(
		InteractionAxisList == EAxisList::Y ? Delta : 0.0,
		InteractionAxisList == EAxisList::Z ? Delta : 0.0,
		InteractionAxisList == EAxisList::X ? Delta : 0.0);

	auto GetCoordinateSystem = [&]()
	{
		if (TransformGizmoSource)
		{
			return TransformGizmoSource->GetGizmoCoordSystemSpace(); 
		}
		return GetGizmoManager()->GetContextQueriesAPI()->GetCurrentCoordinateSystem();
	};
	
	if (GetCoordinateSystem() == EToolContextCoordinateSystem::Local)
	{
		check(ActiveTarget);
		const FMatrix CurrCoordSystem = ActiveTarget->GetTransform().ToMatrixNoScale();
		DeltaRot = (CurrCoordSystem.Inverse() * FRotationMatrix(DeltaRot) * CurrCoordSystem).Rotator();
	}

	return DeltaRot.Quaternion();
}

void UTransformGizmo::OnClickReleaseRotateAxis(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
	bDebugRotate = false;
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
		
		SetModeLastHitPart(EGizmoTransformMode::Rotate, LastHitPart);
	}
}

void UTransformGizmo::OnClickDragScreenSpaceRotate(const FInputDeviceRay& DragPos)
{
	check(GizmoViewContext);
	
	float HitDepth;
	if (GetRayParamIntersectionWithInteractionPlane(DragPos, HitDepth))
	{
		const FVector HitPoint = DragPos.WorldRay.Origin + DragPos.WorldRay.Direction * HitDepth;
		const float HitAngle = GizmoMath::ComputeAngleInPlane(HitPoint,
			InteractionPlanarOrigin, InteractionPlanarNormal, InteractionPlanarAxisX, InteractionPlanarAxisY);

		const FQuat Delta = ComputeAngularRotateDelta(InteractionCurrAngle, HitAngle);
		
		ApplyRotateDelta(Delta);
		InteractionCurrAngle = HitAngle;
	}
}

FQuat UTransformGizmo::ComputeAngularRotateDelta(double InStartAngle, double InEndAngle)
{
	const float DeltaAngle = InEndAngle - InStartAngle;
	return FQuat(InteractionPlanarNormal, DeltaAngle);
}

void UTransformGizmo::OnClickReleaseScreenSpaceRotate(const FInputDeviceRay& InReleasePos)
{
	bInInteraction = false;
}

namespace ArcBallLocals
{	
	/* See Holroyd's implementation that mixes a sphere + and hyperbola to avoid popping
	 * Knud Henriksen, Jon Sporring, and Kasper Hornbaek, “Virtual trackballs revisited”,
	 * IEEE Transactions on Visualization and Computer Graphics, vol. 10, no. 2, pp. 206–216, 2004.
	 */
	bool GetSphereAndHyperbolicProjection(
		const FVector& SphereOrigin,
		const double SphereRadius,
		const FVector& RayOrigin,
		const FVector& RayDirection,
		const UGizmoViewContext& ViewContext,
		FVector& OutProjection)
	{
		const FVector CircleNormal = -ViewContext.GetViewDirection();
		
		// if ray is parallel to circle, no hit
		if (FMath::IsNearlyZero(FVector::DotProduct(CircleNormal, RayDirection)))
		{
			return false;
		}

		const FPlane Plane(SphereOrigin, CircleNormal);
		const double Param = FMath::RayPlaneIntersectionParam(RayOrigin, RayDirection, Plane);

		if (Param < 0)
		{
			return false;
		}

		const FVector HitPoint = RayOrigin + RayDirection * Param;
		FVector Offset = HitPoint - SphereOrigin;

		// switch to screen space
		FVector OffsetProjection = ViewContext.ViewMatrices.GetInvViewMatrix().InverseTransformVector(Offset);

		const double OffsetSquared = Offset.SizeSquared();
		const double CircleRadiusSquared = SphereRadius * SphereRadius;
		
		OffsetProjection.Z = (OffsetSquared <= CircleRadiusSquared * 0.5) ?
			-FMath::Sqrt(CircleRadiusSquared - OffsetSquared) : // spherical projection
			-CircleRadiusSquared * 0.5 / Offset.Length(); // hyperbolic projection

		// switch back to world space
		Offset = ViewContext.ViewMatrices.GetInvViewMatrix().TransformVector(OffsetProjection);
		OutProjection = SphereOrigin + Offset;
		
		return true;
	}
}

void UTransformGizmo::OnClickPressArcBallRotate(const FInputDeviceRay& PressPos)
{
	check(GizmoViewContext);
	
	const FVector& RayOrigin = PressPos.WorldRay.Origin;
	const FVector& RayDir = PressPos.WorldRay.Direction;		
	const double SphereRadius = GetWorldRadius(RotateArcballSphereRadius);

	StartRotation = CurrentRotation = CurrentTransform.GetRotation();
	InteractionPlanarOrigin = CurrentTransform.GetLocation();
	InteractionPlanarNormal = -GizmoViewContext->GetViewDirection();
	InteractionPlanarAxisX = GizmoViewContext->GetViewUp();
	InteractionPlanarAxisY = GizmoViewContext->GetViewRight();
	InteractionAxisList = EAxisList::XYZ;

	auto NeedsInteraction = [&]()
	{
		const bool bIntersect = IntersectionUtil::RaySphereTest(RayOrigin, RayDir, InteractionPlanarOrigin, SphereRadius);
		if (bIntersect)
		{
			return true;
		}
		
		if (bIndirectManipulation)
		{
			// change the arc ball center in indirect manipulation if we didn't hit the sphere
			float HitDepth;
			if (GetRayParamIntersectionWithInteractionPlane(PressPos, HitDepth))
			{
				InteractionPlanarOrigin = PressPos.WorldRay.Origin + PressPos.WorldRay.Direction * HitDepth;
				return true;
			}
		}
		
		return false;
	};

	if (NeedsInteraction())
	{
		// project on sphere
		ArcBallLocals::GetSphereAndHyperbolicProjection(
			InteractionPlanarOrigin, SphereRadius, RayOrigin, RayDir, *GizmoViewContext,
			InteractionArcBallStartPoint);
	
		bInInteraction = true;
	
		SetModeLastHitPart(EGizmoTransformMode::Rotate, LastHitPart);
	}
}

void UTransformGizmo::OnClickDragArcBallRotate(const FInputDeviceRay& DragPos)
{
	const FVector& RayOrigin = DragPos.WorldRay.Origin;
	const FVector& RayDir = DragPos.WorldRay.Direction;		
	const float SphereRadius = GetWorldRadius(RotateArcballSphereRadius);

	// compute projection
	ArcBallLocals::GetSphereAndHyperbolicProjection(
		InteractionPlanarOrigin, SphereRadius, RayOrigin, RayDir, *GizmoViewContext,
		InteractionArcBallCurrPoint);

	if ((InteractionArcBallCurrPoint-InteractionArcBallStartPoint).Length() <= 0.0)
	{
		return;
	}

	// compute rotation
	const FVector Axis1 = (InteractionArcBallCurrPoint - InteractionPlanarOrigin).GetSafeNormal();
	const FVector Axis0 = (InteractionArcBallStartPoint - InteractionPlanarOrigin).GetSafeNormal();
	
	const FQuat DeltaQ = FQuat::FindBetweenNormals(Axis0, Axis1);
	
	// apply rotation
	const FQuat FinalRotation = DeltaQ * StartRotation;
	const FQuat InvCurrentRot = CurrentRotation.Inverse();
	
	const FQuat DeltaRot = (FinalRotation * InvCurrentRot).GetNormalized();
	if (!DeltaRot.IsIdentity())
	{
		ApplyRotateDelta(DeltaRot);
		CurrentRotation = DeltaRot * CurrentRotation; 
	}
}

void UTransformGizmo::OnClickReleaseArcBallRotate(const FInputDeviceRay& ReleasePos)
{
	bInInteraction = false;
}

float UTransformGizmo::GetWorldRadius(const float InRadius) const
{
	const float PixelToWorldScale = GizmoRenderingUtil::CalculateLocalPixelToWorldScale(GizmoViewContext, CurrentTransform.GetLocation());
	const float GizmoScale = TransformGizmoSource ? TransformGizmoSource->GetGizmoScale() : 1.0f;
	return InRadius * GetSizeCoefficient() * PixelToWorldScale * GizmoScale;
}

float UTransformGizmo::GetSizeCoefficient() const
{
	return CustomizationFunction ? CustomizationFunction().SizeCoefficient : 1.5f;
}

FVector2D UTransformGizmo::GetScreenProjectedAxis(const UGizmoViewContext* View, const FVector& InLocalAxis, const FTransform& InLocalToWorld)
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
	const FQuat NewRotation = InRotateDelta * CurrentTransform.GetRotation();
	CurrentTransform.SetRotation(NewRotation);
	ActiveTarget->SetTransform(CurrentTransform);
}

void UTransformGizmo::ApplyScaleDelta(const FVector& InScaleDelta)
{
	const FVector StartScale = CurrentTransform.GetScale3D();
	const FVector NewScale = StartScale + InScaleDelta;
	CurrentTransform.SetScale3D(NewScale);
	ActiveTarget->SetTransform(CurrentTransform);
}

#undef LOCTEXT_NAMESPACE
