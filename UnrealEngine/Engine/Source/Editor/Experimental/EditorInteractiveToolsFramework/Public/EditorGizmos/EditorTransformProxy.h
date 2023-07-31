// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseGizmos/TransformProxy.h"
#include "CoreMinimal.h"
#include "Math/Axis.h"
#include "Math/MathFwd.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Misc/AssertionMacros.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "EditorTransformProxy.generated.h"

class UObject;

/**
 * UEditorTransformProxy is a derivation of UTransformProxy that
 * returns the transform that defines the current space of the default
 * Editor transform gizmo for a given mode manager / viewport.
 * 
 * @todo Currently this defaults internally to GLevelEditorModeManager()
 * but eventually it should be possible to set and use a different mode
 * manager.
 */
UCLASS(Transient)
class EDITORINTERACTIVETOOLSFRAMEWORK_API UEditorTransformProxy : public UTransformProxy
{
	GENERATED_BODY()
public:

	/**
	 * @return the stored transform for currently selected objects.
	 */
	virtual FTransform GetTransform() const override;

	/**
	 * Unimplemented - all updates to the Editor transform proxy MUST be made by calling the Input delta methods.
	 */
	virtual void SetTransform(const FTransform& Transform) override 
	{
		check(false);
	}

	/** Input translate delta to be applied in world space of the current transform. */
	virtual void InputTranslateDelta(const FVector& InDeltaTranslate, EAxisList::Type InAxisList);

	/** Input rotation delta to be applied in local space of the current transform. */
	virtual void InputRotateDelta(const FRotator& InDeltaRotate, EAxisList::Type InAxisList);

	/** Input scale delta to be applied in local space of the current transform. */
	virtual void InputScaleDelta(const FVector& InDeltaScale, EAxisList::Type InAxisList);
};

