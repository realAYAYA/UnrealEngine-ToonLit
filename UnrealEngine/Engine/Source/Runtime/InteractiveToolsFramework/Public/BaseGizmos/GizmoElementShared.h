// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GizmoElementShared.generated.h"

//
// Visible/hittable state of gizmo element
//
UENUM()
enum class EGizmoElementState : uint8
{
	None		= 0x00,
	Visible		= 1<<1,
	Hittable	= 1<<2,
	VisibleAndHittable = Visible | Hittable
};

ENUM_CLASS_FLAGS(EGizmoElementState)

//
// Interaction state of gizmo element
//
UENUM()
enum class EGizmoElementInteractionState
{
	None,
	Hovering,
	Interacting
};

//
// View dependent type: automatically cull gizmo element based on view.
//
//   Axis  - Cull object when angle between axis and view direction is within a given tolerance
//   Plane - Cull object when angle between plane normal and view direction is perpendicular within a given tolerance
//
UENUM()
enum class EGizmoElementViewDependentType
{
	None,
	Axis,
	Plane
};

//
// View align type: automatically align gizmo element towards a view.
// 
//   PointOnly   - Align object forward axis to view direction only, useful for symmetrical objects such as a circle
//   PointEye    - Align object forward axis to -camera view direction (camera pos - object center), align object up axis to scene view up
//   PointScreen - Align object forward axis to scene view forward direction (view up ^ view right), align object up axis to scene view up
//   Axial		- Rotate object around up axis, minimizing angle between forward axis and view direction
//
UENUM()
enum class EGizmoElementViewAlignType
{
	None,
	PointOnly,
	PointEye,
	PointScreen,
	Axial
};


//
// Partial type: render partial element for those elements which support it.
// 
//   Partial				- Render partial element.
//   PartialViewDependent   - Render partial unless view direction aligns with an axis or normal specified by the element type.
//
UENUM()
enum class EGizmoElementPartialType
{
	None,
	Partial,
	PartialViewDependent
};

