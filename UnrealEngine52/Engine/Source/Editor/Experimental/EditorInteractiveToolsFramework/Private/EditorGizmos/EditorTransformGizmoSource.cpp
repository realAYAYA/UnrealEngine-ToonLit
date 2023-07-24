// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/EditorTransformGizmoSource.h"

#include "Editor.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"

EGizmoTransformMode FEditorTransformGizmoUtil::GetGizmoMode(UE::Widget::EWidgetMode InWidgetMode)
{
	switch (InWidgetMode)
	{
		case UE::Widget::EWidgetMode::WM_Translate: return EGizmoTransformMode::Translate;
		case UE::Widget::EWidgetMode::WM_Rotate: return EGizmoTransformMode::Rotate;
		case UE::Widget::EWidgetMode::WM_Scale: return EGizmoTransformMode::Scale;
	}
	return EGizmoTransformMode::None;
}

UE::Widget::EWidgetMode FEditorTransformGizmoUtil::GetWidgetMode(EGizmoTransformMode InGizmoMode)
{
	switch (InGizmoMode)
	{
		case EGizmoTransformMode::Translate: return UE::Widget::EWidgetMode::WM_Translate;
		case EGizmoTransformMode::Rotate: return UE::Widget::EWidgetMode::WM_Rotate;
		case EGizmoTransformMode::Scale: return UE::Widget::EWidgetMode::WM_Scale;
	}
	return UE::Widget::EWidgetMode::WM_None;
}

EGizmoTransformMode UEditorTransformGizmoSource::GetGizmoMode() const
{
	if (FEditorViewportClient* ViewportClient = GetViewportClient())
	{
		UE::Widget::EWidgetMode WidgetMode = ViewportClient->GetWidgetMode();
		return FEditorTransformGizmoUtil::GetGizmoMode(WidgetMode);
	}
	return EGizmoTransformMode::None;
}

EAxisList::Type UEditorTransformGizmoSource::GetGizmoAxisToDraw(EGizmoTransformMode InGizmoMode) const
{ 
	if (FEditorViewportClient* ViewportClient = GetViewportClient())
	{
		UE::Widget::EWidgetMode WidgetMode = ViewportClient->GetWidgetMode();
		return GetModeTools().GetWidgetAxisToDraw(WidgetMode);
	}
	return EAxisList::None;
}

EToolContextCoordinateSystem UEditorTransformGizmoSource::GetGizmoCoordSystemSpace() const
{
	FEditorViewportClient* ViewportClient = GetViewportClient();
	if (ViewportClient && ViewportClient->GetWidgetCoordSystemSpace() == ECoordSystem::COORD_Local)
	{
		return EToolContextCoordinateSystem::Local;
	}
	else
	{
		return EToolContextCoordinateSystem::World;
	}
}

float UEditorTransformGizmoSource::GetGizmoScale() const
{
	return GetModeTools().GetWidgetScale();
}


bool UEditorTransformGizmoSource::GetVisible() const
{
	if (FEditorViewportClient* ViewportClient = GetViewportClient()) 
	{
		if (GetModeTools().GetShowWidget() && GetModeTools().UsesTransformWidget())
		{
			UE::Widget::EWidgetMode WidgetMode = ViewportClient->GetWidgetMode();
			bool bUseLegacyWidget = (WidgetMode == UE::Widget::WM_TranslateRotateZ || WidgetMode == UE::Widget::WM_2D);
			if (!bUseLegacyWidget)
			{
				static IConsoleVariable* const UseLegacyWidgetCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Gizmos.UseLegacyWidget"));
				if (ensure(UseLegacyWidgetCVar))
				{
					bUseLegacyWidget = UseLegacyWidgetCVar->GetInt() > 0;
				}
			}

			return !bUseLegacyWidget;
		}
	}

	return false;
}

EGizmoTransformScaleType UEditorTransformGizmoSource::GetScaleType() const
{
	if (GEditor->UsePercentageBasedScaling())
	{
		return EGizmoTransformScaleType::PercentageBased;
	}

	return EGizmoTransformScaleType::Default;
}

FEditorModeTools& UEditorTransformGizmoSource::GetModeTools() const
{
	return GLevelEditorModeTools();
}

FEditorViewportClient* UEditorTransformGizmoSource::GetViewportClient() const
{
	return GetModeTools().GetFocusedViewportClient();
}