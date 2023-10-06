// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DebugRenderSceneProxy.h"

/**
 * Simple DebugRenderSceneProxy that gets relevant when associated component is shown and view flag is active (if specified on construction)
 */
#if UE_ENABLE_DEBUG_DRAWING
class SMARTOBJECTSMODULE_API FSmartObjectDebugSceneProxy final : public FDebugRenderSceneProxy
{
public:
	explicit FSmartObjectDebugSceneProxy(const UPrimitiveComponent& InComponent, const EDrawType InDrawType = EDrawType::WireMesh, const TCHAR* InViewFlagName = nullptr);

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override;

private:
	uint32 ViewFlagIndex = 0;
};
#endif // UE_ENABLE_DEBUG_DRAWING
