// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"
#include "VisualLoggerRenderingComponent.generated.h"

/**
 *	Primitive component used to draw visual logger data on level
 */
UCLASS(ClassGroup = Debug)
class UVisualLoggerRenderingComponent : public UDebugDrawComponent
{
public:
	GENERATED_UCLASS_BODY()

	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
};
