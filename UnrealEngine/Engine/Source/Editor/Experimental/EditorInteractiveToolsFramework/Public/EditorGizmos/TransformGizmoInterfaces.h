// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolContextInterfaces.h"
#include "UnrealWidgetFwd.h"
#include "TransformGizmoInterfaces.generated.h"

class UMaterial;

//
// Interface for the Transform gizmo.
//
UENUM()
enum class EGizmoTransformMode : uint8
{
	None = 0,
	Translate,
	Rotate,
	Scale,
	Max
};

UENUM()
enum class EGizmoTransformScaleType : uint8
{
	Default,
	PercentageBased
};

UINTERFACE()
class EDITORINTERACTIVETOOLSFRAMEWORK_API UTransformGizmoSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * ITransformGizmoSource is an interface for providing gizmo mode configuration information.
 */
class EDITORINTERACTIVETOOLSFRAMEWORK_API ITransformGizmoSource
{
	GENERATED_BODY()
public:
	/**
	 * Returns the current Editor gizmo mode
	 */
	virtual EGizmoTransformMode GetGizmoMode() const PURE_VIRTUAL(ITransformGizmoSource::GetGizmoMode, return EGizmoTransformMode::None;);

	/**
	 * Returns the current gizmo axes to draw
	 */
	virtual EAxisList::Type GetGizmoAxisToDraw(EGizmoTransformMode InGizmoMode) const PURE_VIRTUAL(ITransformGizmoSource::GetGizmoAxisToDraw, return EAxisList::None;);

	/**
	 * Returns the coordinate system space (world or local) in which to display the gizmo
	 */
	virtual EToolContextCoordinateSystem GetGizmoCoordSystemSpace() const PURE_VIRTUAL(ITransformGizmoSource::GetGizmoCoordSystemSpace, return EToolContextCoordinateSystem::World;);

	/**
	 * Returns a scale factor for the gizmo
	 */
	virtual float GetGizmoScale() const PURE_VIRTUAL(ITransformGizmoSource::GetGizmoScale, return 1.0f;);

	/* 
	 * Returns whether the gizmo should be visible. 
	 */
	virtual bool GetVisible() const PURE_VIRTUAL(ITransformGizmoSource::GetVisible, return false;);

	/* 
	 * Returns whether the gizmo can interact.
	 */
	virtual bool CanInteract() const PURE_VIRTUAL(ITransformGizmoSource::CanInteract, return false;);

	/*
	 * Get current scale type
	 */
	virtual EGizmoTransformScaleType GetScaleType() const PURE_VIRTUAL(ITransformGizmoSource::GetScaleType, return EGizmoTransformScaleType::Default;);
};

/**
 * FGizmoCustomization is a struct that can be used to make some display overrides (currently material and size) 
 */

struct FGizmoCustomization
{
	TObjectPtr<UMaterial> Material = nullptr;
	float SizeCoefficient = 1.f;
};
