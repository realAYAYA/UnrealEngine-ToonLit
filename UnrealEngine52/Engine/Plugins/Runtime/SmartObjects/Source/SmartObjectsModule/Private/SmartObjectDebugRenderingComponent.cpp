// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDebugRenderingComponent.h"
#include "SmartObjectDebugSceneProxy.h"
#include "Debug/DebugDrawService.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectDebugRenderingComponent)

USmartObjectDebugRenderingComponent::USmartObjectDebugRenderingComponent(const FObjectInitializer& ObjectInitialize)
	: Super(ObjectInitialize)
{
#if WITH_EDITORONLY_DATA
	HitProxyPriority = HPP_Wireframe;
#endif
}

#if UE_ENABLE_DEBUG_DRAWING
void USmartObjectDebugRenderingComponent::OnRegister()
{
	Super::OnRegister();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		CanvasDebugDrawDelegateHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &USmartObjectDebugRenderingComponent::DebugDrawCanvas));
	}
}

void USmartObjectDebugRenderingComponent::OnUnregister()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UDebugDrawService::Unregister(CanvasDebugDrawDelegateHandle);
	}

	Super::OnUnregister();
}

FDebugRenderSceneProxy* USmartObjectDebugRenderingComponent::CreateDebugSceneProxy()
{
	FSmartObjectDebugSceneProxy* DebugProxy = new FSmartObjectDebugSceneProxy(*this, FDebugRenderSceneProxy::WireMesh, *ViewFlagName);
	DebugDraw(DebugProxy);
	return DebugProxy;
}
#endif // UE_ENABLE_DEBUG_DRAWING
