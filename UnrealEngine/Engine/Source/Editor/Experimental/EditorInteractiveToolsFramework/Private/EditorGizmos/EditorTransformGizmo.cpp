// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorGizmos/EditorTransformGizmo.h"

#include "Containers/EnumAsByte.h"
#include "EditorGizmos/EditorTransformProxy.h"
#include "Logging/LogMacros.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"

#define LOCTEXT_NAMESPACE "UEditorTransformGizmo"

DEFINE_LOG_CATEGORY_STATIC(LogEditorTransformGizmo, Log, All);

void UEditorTransformGizmo::ApplyTranslateDelta(const FVector& InTranslateDelta)
{
	check(ActiveTarget);

	if (UEditorTransformProxy* EditorTransformProxy = Cast<UEditorTransformProxy>(ActiveTarget))
	{
		EditorTransformProxy->InputTranslateDelta(InTranslateDelta, InteractionAxisList);

		// Update the cached current transform
		CurrentTransform.AddToTranslation(InTranslateDelta);
	}
	else
	{
		Super::ApplyTranslateDelta(InTranslateDelta);
	}
}

void UEditorTransformGizmo::ApplyRotateDelta(const FQuat& InRotateDelta)
{
	check(ActiveTarget);

	if (UEditorTransformProxy* EditorTransformProxy = Cast<UEditorTransformProxy>(ActiveTarget))
	{
		// Update the cached current delta.
		// Applies rot delta after the current rotation.
		FQuat NewRotation = InRotateDelta * CurrentTransform.GetRotation();
		CurrentTransform.SetRotation(NewRotation);

		EditorTransformProxy->InputRotateDelta(InRotateDelta.Rotator(), InteractionAxisList);
	}
	else
	{
		Super::ApplyRotateDelta(InRotateDelta);
	}
}

void UEditorTransformGizmo::ApplyScaleDelta(const FVector& InScaleDelta)
{
	check(ActiveTarget);

	if (UEditorTransformProxy* EditorTransformProxy = Cast<UEditorTransformProxy>(ActiveTarget))
	{
		FVector StartScale = CurrentTransform.GetScale3D();

		EditorTransformProxy->InputScaleDelta(InScaleDelta, InteractionAxisList);

		// Update the cached current transform
		FVector NewScale = StartScale + InScaleDelta;
		CurrentTransform.SetScale3D(NewScale);
	}
	else
	{
		Super::ApplyScaleDelta(InScaleDelta);
	}
}

#undef LOCTEXT_NAMESPACE
