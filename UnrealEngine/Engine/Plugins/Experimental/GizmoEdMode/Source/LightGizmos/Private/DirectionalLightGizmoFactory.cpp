// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirectionalLightGizmoFactory.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/TransformProxy.h"
#include "Math/Rotator.h"
#include "UnrealWidgetFwd.h"
#include "Engine/DirectionalLight.h"
#include "DirectionalLightGizmo.h"
#include "LightGizmosModule.h"
#include "SubTransformProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DirectionalLightGizmoFactory)

bool UDirectionalLightGizmoFactory::CanBuildGizmoForSelection(FEditorModeTools* ModeTools) const
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
		if (!Cast<ADirectionalLight>(Actor))
		{
			return false;
		}
	}

	// Only return true if all selected Actors are Directional Lights
	return true;
}

TArray<UInteractiveGizmo*> UDirectionalLightGizmoFactory::BuildGizmoForSelection(FEditorModeTools* ModeTools, UInteractiveGizmoManager* GizmoManager) const
{
	TArray<UInteractiveGizmo*> Gizmos;
	TArray<AActor*> SelectedActors;
	ModeTools->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

	// Createa Light Gizmo for each actor in selection
	for (AActor* Actor : SelectedActors)
	{
		UDirectionalLightGizmo* LightGizmo = Cast<UDirectionalLightGizmo>(GizmoManager->CreateGizmo(FLightGizmosModule::DirectionalLightGizmoType));

		LightGizmo->SetSelectedObject(Cast<ADirectionalLight>(Actor));

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
		Elements = ETransformGizmoSubElements::None; // Custom Rotation in DirectionalLightGizmo
		break;
	case UE::Widget::EWidgetMode::WM_Scale:
		Elements = ETransformGizmoSubElements::None; // Scaling doesn't make sense for a directional light
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
		UDirectionalLightGizmo* LightGizmo = Cast<UDirectionalLightGizmo>(Gizmo);

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

void UDirectionalLightGizmoFactory::ConfigureGridSnapping(bool bGridEnabled, bool bRotGridEnabled, const TArray<UInteractiveGizmo*>& Gizmos) const
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

