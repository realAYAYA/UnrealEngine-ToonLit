// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDebugSceneProxy.h"
#include "PrimitiveViewRelevance.h"

#if UE_ENABLE_DEBUG_DRAWING
FSmartObjectDebugSceneProxy::FSmartObjectDebugSceneProxy(const UPrimitiveComponent& InComponent, const EDrawType InDrawType, const TCHAR* InViewFlagName)
	: FDebugRenderSceneProxy(&InComponent)
{
	DrawType = InDrawType;
	ViewFlagName = InViewFlagName;
	ViewFlagIndex = uint32(FEngineShowFlags::FindIndexByName(*ViewFlagName));
}

FPrimitiveViewRelevance FSmartObjectDebugSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && (ViewFlagIndex == INDEX_NONE || View->Family->EngineShowFlags.GetSingleFlag(ViewFlagIndex));
	Result.bDynamicRelevance = true;
	Result.bSeparateTranslucency = Result.bNormalTranslucency = true;
	return Result;
}

uint32 FSmartObjectDebugSceneProxy::GetMemoryFootprint(void) const
{
	return sizeof(*this) + FDebugRenderSceneProxy::GetAllocatedSize();
}
#endif // UE_ENABLE_DEBUG_DRAWING
