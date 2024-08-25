// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorInteractiveGizmoSelectionBuilder.h"
#include "InteractiveGizmoBuilder.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "EditorGizmos/TransformGizmoInterfaces.h"

#include "EditorTransformGizmoBuilder.generated.h"

class UInteractiveGizmo;
class UObject;
class UEditorTransformGizmoContextObject;
struct FToolBuilderState;

UCLASS()
class EDITORINTERACTIVETOOLSFRAMEWORK_API UEditorTransformGizmoBuilder : public UInteractiveGizmoBuilder, public IEditorInteractiveGizmoSelectionBuilder
{
	GENERATED_BODY()

public:

	// UEditorInteractiveGizmoSelectionBuilder interface 
	virtual UInteractiveGizmo* BuildGizmo(const FToolBuilderState& SceneState) const override;
	virtual void UpdateGizmoForSelection(UInteractiveGizmo* Gizmo, const FToolBuilderState& SceneState) override;

	// If set, this function will be passed to UTransformGizmo instances to override the default material and display size.
	TFunction<const FGizmoCustomization()> CustomizationFunction;

private:
	static const UEditorTransformGizmoContextObject* GetTransformGizmoContext(const FToolBuilderState& InSceneState);
};


