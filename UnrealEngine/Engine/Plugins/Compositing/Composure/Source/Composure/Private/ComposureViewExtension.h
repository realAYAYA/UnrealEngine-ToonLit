// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "SceneViewExtension.h"
#include "UObject/WeakObjectPtr.h"

class AComposurePipelineBaseActor;

/**
 *	
 */
class FComposureViewExtension : public FSceneViewExtensionBase
{
public:
	FComposureViewExtension(const FAutoRegister& AutoRegister, AComposurePipelineBaseActor* Owner);

public:
	//~ ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& RHICmdList, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& RHICmdList, FSceneViewFamily& InViewFamily) override {}
	virtual int32 GetPriority() const override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	TWeakObjectPtr<AComposurePipelineBaseActor> AssociatedPipelineObj;
};