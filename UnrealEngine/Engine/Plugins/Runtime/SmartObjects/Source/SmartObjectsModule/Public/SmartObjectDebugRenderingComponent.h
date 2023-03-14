// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Debug/DebugDrawComponent.h"
#include "SmartObjectDebugRenderingComponent.generated.h"

/**
 * Simple UDebugDrawComponent to inherit from to use a FSmartObjectDebugSceneProxy.
 * Derived classes can set ViewFlagName at construction to control relevancy.
 */
UCLASS(ClassGroup = Debug, NotBlueprintable, NotPlaceable)
class SMARTOBJECTSMODULE_API USmartObjectDebugRenderingComponent : public UDebugDrawComponent
{
	GENERATED_BODY()
public:
	explicit USmartObjectDebugRenderingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
#if UE_ENABLE_DEBUG_DRAWING
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual void DebugDraw(FDebugRenderSceneProxy* DebugProxy) {}
	virtual void DebugDrawCanvas(UCanvas* Canvas, APlayerController*) {}

	FDelegateHandle CanvasDebugDrawDelegateHandle;
	FString ViewFlagName;
#endif // UE_ENABLE_DEBUG_DRAWING
};

