// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "GizmoEdModeInterface.generated.h"

/**
 * Gizmo state structure used to pass gizmo data if needed
 */

struct FGizmoState
{
};

/**
 * Interface for the new editor TRS gizmos
 */

UINTERFACE(NotBlueprintable, MinimalAPI)
class UGizmoEdModeInterface : public UInterface
{
	GENERATED_BODY()
};

class EDITORINTERACTIVETOOLSFRAMEWORK_API IGizmoEdModeInterface
{
	GENERATED_BODY()

public:
	
	virtual bool BeginTransform(const FGizmoState& InState) = 0;
	virtual bool EndTransform(const FGizmoState& InState) = 0;
};