// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorGizmos/TransformGizmo.h"
#include "Math/MathFwd.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "EditorTransformGizmo.generated.h"

class UObject;

/**
 * UEditorTransformGizmo handles Editor-specific functionality for the TransformGizmo,
 * applied to a UEditorTransformProxy target object.
 */
UCLASS()
class EDITORINTERACTIVETOOLSFRAMEWORK_API UEditorTransformGizmo : public UTransformGizmo
{
	GENERATED_BODY()

	/**  UTransformGizmo override */
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	
protected:

	/** Apply translate delta to transform proxy */
	virtual void ApplyTranslateDelta(const FVector& InTranslateDelta) override;

	/** Apply rotate delta to transform proxy */
	virtual void ApplyRotateDelta(const FQuat& InRotateDelta) override;

	/** Apply scale delta to transform proxy */
	virtual void ApplyScaleDelta(const FVector& InScaleDelta) override;

	/**  UTransformGizmo override */
	virtual void SetActiveTarget(
		UTransformProxy* Target,
		IToolContextTransactionProvider* TransactionProvider = nullptr,
		IGizmoStateTarget* InStateTarget = nullptr) override;

	/**
	 * Functions to listen to gizmo transform begin/end.
	 * They are currently used to set data on the legacy widget as some viewport clients rely on current axis to be set.
	 */
	void OnGizmoTransformBegin(UTransformProxy* TransformProxy) const;
	void OnGizmoTransformEnd(UTransformProxy* TransformProxy) const;;
};