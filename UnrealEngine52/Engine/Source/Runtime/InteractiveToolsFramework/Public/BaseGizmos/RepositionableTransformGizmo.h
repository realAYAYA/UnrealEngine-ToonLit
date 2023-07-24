// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BaseGizmos/CombinedTransformGizmo.h"
#include "InteractiveGizmoBuilder.h"
#include "ToolContextInterfaces.h"

#include "RepositionableTransformGizmo.generated.h"

class IToolContextTransactionProvider;
class UTransformProxy;

UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API URepositionableTransformGizmoBuilder : public UCombinedTransformGizmoBuilder
{
	GENERATED_BODY()

public:

	virtual UInteractiveGizmo* BuildGizmo(const FToolBuilderState& SceneState) const override;
};


/**
 * A transform gizmo that also allows the user to reposition it by middle-clicking rotation/translation components.
 */
UCLASS()
class INTERACTIVETOOLSFRAMEWORK_API URepositionableTransformGizmo : public UCombinedTransformGizmo
{
	GENERATED_BODY()

public:

	/**
	 * Set the active target object for the Gizmo
	 * @param Target active target
	 * @param TransactionProvider optional IToolContextTransactionProvider implementation to use - by default uses GizmoManager
	 */
	virtual void SetActiveTarget(UTransformProxy* Target, IToolContextTransactionProvider* TransactionProvider = nullptr) override;

	/**
	 * Allows the user to provide functions to use for aligning the gizmo destination to items in the scene. Note that this affects
	 * both the movement and repositioning.
	 */
	virtual void SetWorldAlignmentFunctions(
		TUniqueFunction<bool()>&& ShouldAlignTranslationIn,
		TUniqueFunction<bool(const FRay&, FVector&)>&& TranslationAlignmentRayCasterIn) override;

	/**
	 * Allows the user to provide functions to use in aligning the gizmo during repositioning only.
	 * This is because the repositining is likely to want different raycasts that don't ignore the 
	 * moved components themselves, so that the gizmo can be aligned to a target component.
	 */
	virtual void SetPivotAlignmentFunctions(
		TUniqueFunction<bool()>&& ShouldAlignPivotIn,
		TUniqueFunction<bool(const FRay&, FVector&)>&& PivotAlignmentRayCasterIn);
protected:

	TUniqueFunction<bool()> ShouldAlignPivot = []() { return false; };
	TUniqueFunction<bool(const FRay&, FVector&)> PivotAlignmentRayCaster = [](const FRay&, FVector&) {return false; };

	/** Subset of ActiveGizmos, for use by SetPivotAlignmentFunctions */
	TArray<UInteractiveGizmo*> PivotAlignmentGizmos;

	UPROPERTY()
	TObjectPtr<UGizmoTransformChangeStateTarget> RepositionStateTarget;

	// Helper functions
	void ModifyPivotAxisGizmo(UInteractiveGizmo* SubGizmoIn);
	void ModifyPivotPlaneGizmo(UInteractiveGizmo* SubGizmoIn);
	void ModifyPivotRotateGizmo(UInteractiveGizmo* SubGizmoIn);
};
