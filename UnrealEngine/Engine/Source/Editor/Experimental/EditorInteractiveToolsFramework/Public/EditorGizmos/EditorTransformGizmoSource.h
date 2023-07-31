// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorGizmos/TransformGizmoInterfaces.h"
#include "Math/Axis.h"
#include "ToolContextInterfaces.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealWidgetFwd.h"

#include "EditorTransformGizmoSource.generated.h"

class FEditorModeTools;
class FEditorViewportClient;
class FSceneView;

namespace FEditorTransformGizmoUtil
{
	/** Convert UE::Widget::EWidgetMode to ETransformGizmoMode*/
	EGizmoTransformMode GetGizmoMode(UE::Widget::EWidgetMode InWidgetMode);

	/** Convert EEditorGizmoMode to UE::Widget::EWidgetMode*/
	UE::Widget::EWidgetMode GetWidgetMode(EGizmoTransformMode InGizmoMode);
};

/**
 * UEditorTransformGizmoSource is an ITransformGizmoSource implementation that provides
 * current state information used to configure the Editor transform gizmo.
 */
UCLASS()
class EDITORINTERACTIVETOOLSFRAMEWORK_API UEditorTransformGizmoSource : public UObject, public ITransformGizmoSource
{
	GENERATED_BODY()
public:
	
	/**
	 * @return The current display mode for the Editor transform gizmo
	 */
	virtual EGizmoTransformMode GetGizmoMode() const;

	/**
	 * @return The current axes to draw for the specified mode
	 */
	virtual EAxisList::Type GetGizmoAxisToDraw(EGizmoTransformMode InWidgetMode) const;

	/**
	 * @return The coordinate system space (world or local) to display the widget in
	 */
	virtual EToolContextCoordinateSystem GetGizmoCoordSystemSpace() const;

	/**
	 * Returns a scale factor for the gizmo
	 */
	virtual float GetGizmoScale() const;

	/**
	 * Whether the gizmo is visible
	 */
	virtual bool GetVisible() const;

	/**
 	 * Get current scale type
	 */
	virtual EGizmoTransformScaleType GetScaleType() const;

public:
	static UEditorTransformGizmoSource* Construct(
		UObject* Outer = (UObject*)GetTransientPackage())
	{
		return NewObject<UEditorTransformGizmoSource>(Outer);
	}

protected:

	FEditorModeTools& GetModeTools() const;

	FEditorViewportClient* GetViewportClient() const;
};

