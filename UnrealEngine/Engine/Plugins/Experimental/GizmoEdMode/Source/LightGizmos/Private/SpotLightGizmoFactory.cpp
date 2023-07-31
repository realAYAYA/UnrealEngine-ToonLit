// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpotLightGizmoFactory.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "SubTransformProxy.h"
#include "Math/Rotator.h"
#include "UnrealWidgetFwd.h"
#include "Engine/SpotLight.h"
#include "SpotLightGizmo.h"
#include "LightGizmosModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SpotLightGizmoFactory)

bool USpotLightGizmoFactory::CanBuildGizmoForSelection(FEditorModeTools* ModeTools) const
{
	/** Since there can only be one factory active at a time in the current iteration
	 *  of the tools framework, this function only returns true if /all/ items in the
	 *  current selection are point lights and then creates point light gizmos for them
	 *  If not, the selection falls through to the next factory
	 */
	TArray<AActor*> SelectedActors;
	ModeTools->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

	for (const AActor* Actor : SelectedActors)
	{
		if (!Cast<ASpotLight>(Actor))
		{
			return false;
		}
	}

	// Only return true if all actors in selection are SpotLights
	return true;
}

TArray<UInteractiveGizmo*> USpotLightGizmoFactory::BuildGizmoForSelection(FEditorModeTools* ModeTools, UInteractiveGizmoManager* GizmoManager) const
{
	TArray<UInteractiveGizmo*> Gizmos;
	TArray<AActor*> SelectedActors;
	ModeTools->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

	bool bCreateInnerConeGizmo = true;

	 // Don't create an inner cone if too many actors are selected to avoid viewport clutter
	if (SelectedActors.Num() > 5)
	{
		bCreateInnerConeGizmo = false;
	}

	// Create a USpotLightGizmo for each actor in the selection
	for (AActor* Actor : SelectedActors)
	{
		USpotLightGizmo* LightGizmo = Cast<USpotLightGizmo>(GizmoManager->CreateGizmo(FLightGizmosModule::SpotLightGizmoType));
		LightGizmo->SetSelectedObject(Cast<ASpotLight>(Actor));
		LightGizmo->CreateOuterAngleGizmo();

		if (bCreateInnerConeGizmo)
		{
			LightGizmo->CreateInnerAngleGizmo();
			LightGizmo->CreateAttenuationScaleGizmo();
		}
		Gizmos.Add(LightGizmo);
	}

	// Create one transform gizmo for the whole selection
	ETransformGizmoSubElements Elements = ETransformGizmoSubElements::None;
	bool bUseContextCoordinateSystem = true;
	UE::Widget::EWidgetMode WidgetMode = ModeTools->GetWidgetMode();

	switch (WidgetMode)
	{
	case UE::Widget::EWidgetMode::WM_Translate:
		Elements = ETransformGizmoSubElements::TranslateAllAxes | ETransformGizmoSubElements::TranslateAllPlanes;
		break;
	case UE::Widget::EWidgetMode::WM_Rotate:
		Elements = ETransformGizmoSubElements::RotateAllAxes;
		break;
	case UE::Widget::EWidgetMode::WM_Scale:
		Elements = ETransformGizmoSubElements::None; // Attenuation can be scaled using custom gizmo anyways, so scaling doesn't make sense
		bUseContextCoordinateSystem = false;
		break;
	case UE::Widget::EWidgetMode::WM_2D:
		Elements = ETransformGizmoSubElements::RotateAxisY | ETransformGizmoSubElements::TranslatePlaneXZ;
		break;
	default:
		Elements = ETransformGizmoSubElements::FullTranslateRotateScale;
		break;
	}
	UCombinedTransformGizmo* TransformGizmo = GizmoManager->CreateCustomTransformGizmo(Elements);
	TransformGizmo->bUseContextCoordinateSystem = bUseContextCoordinateSystem;

	USubTransformProxy* TransformProxy = NewObject<USubTransformProxy>();

	// Get the SubTransformProxy from each UDirectionalLightGizmo and connect it to the main Transform Proxy
	for (UInteractiveGizmo* Gizmo : Gizmos)
	{
		USpotLightGizmo* LightGizmo = Cast<USpotLightGizmo>(Gizmo);

		if (LightGizmo)
		{
			TransformProxy->AddSubTransformProxy(LightGizmo->GetTransformProxy());
		}

	}

	TransformGizmo->SetActiveTarget(TransformProxy);
	TransformGizmo->SetVisibility(SelectedActors.Num() > 0);

	Gizmos.Add(TransformGizmo);

	return Gizmos;
}

void USpotLightGizmoFactory::ConfigureGridSnapping(bool bGridEnabled, bool bRotGridEnabled, const TArray<UInteractiveGizmo*>& Gizmos) const
{
	for (UInteractiveGizmo* Gizmo : Gizmos)
	{
		UCombinedTransformGizmo* TransformGizmo = Cast<UCombinedTransformGizmo>(Gizmo);

		if (TransformGizmo)
		{
			TransformGizmo->bSnapToWorldGrid = bGridEnabled;
			TransformGizmo->bSnapToWorldRotGrid = bRotGridEnabled;
		}
	}
}



