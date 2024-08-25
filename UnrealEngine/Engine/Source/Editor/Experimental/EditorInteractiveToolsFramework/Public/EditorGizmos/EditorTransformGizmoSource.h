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

class UEditorTransformGizmoContextObject;
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
	virtual EGizmoTransformMode GetGizmoMode() const override;

	/**
	 * @return The current axes to draw for the specified mode
	 */
	virtual EAxisList::Type GetGizmoAxisToDraw(EGizmoTransformMode InWidgetMode) const override;

	/**
	 * @return The coordinate system space (world or local) to display the widget in
	 */
	virtual EToolContextCoordinateSystem GetGizmoCoordSystemSpace() const override;

	/**
	 * Returns a scale factor for the gizmo
	 */
	virtual float GetGizmoScale() const override;

	/**
	 * Whether the gizmo is visible
	 */
	virtual bool GetVisible() const override;

	/* 
	 * Returns whether the gizmo can interact.
	 * Note that this can be true even if the gizmo is hidden to support indirect manipulation in game mode.
	 */
	virtual bool CanInteract() const override;

	/**
 	 * Get current scale type
	 */
	virtual EGizmoTransformScaleType GetScaleType() const override;

	static UEditorTransformGizmoSource* CreateNew(
		UObject* Outer = (UObject*)GetTransientPackage(),
		const UEditorTransformGizmoContextObject* InContext = nullptr);

protected:

	const FEditorModeTools& GetModeTools() const;

	const FEditorViewportClient* GetViewportClient() const;
	
	TWeakObjectPtr<const UEditorTransformGizmoContextObject> WeakContext = nullptr;
};

