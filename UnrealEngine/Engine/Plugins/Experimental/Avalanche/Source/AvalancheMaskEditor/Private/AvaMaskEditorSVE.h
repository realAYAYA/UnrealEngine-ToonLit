// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"

class UGeometryMaskCanvas;

class FAvaMaskSceneViewExtension
	: public FWorldSceneViewExtension
	, public FGCObject
{
public:
	FAvaMaskSceneViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld);

	// ~Begin ISceneViewExtension
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override { }
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	// ~End ISceneViewExtension

	// ~Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	virtual FString GetReferencerName() const override;
	// ~End FGCObject Interface

private:
	TSoftObjectPtr<UMaterialInterface> PostProcessMaterial;
	TObjectPtr<UMaterialInstanceDynamic> PostProcessMaterialMID;
};

