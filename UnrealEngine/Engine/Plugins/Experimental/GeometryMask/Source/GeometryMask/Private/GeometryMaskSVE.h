// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"

class UGeometryMaskSubsystem;
class UGeometryMaskCanvas;

class FGeometryMaskSceneViewExtension
	: public FWorldSceneViewExtension
{
public:
	FGeometryMaskSceneViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld);

	// ~Begin ISceneViewExtension
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override { }
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override { }
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	// ~End ISceneViewExtension

private:
	TWeakObjectPtr<UGeometryMaskSubsystem> GeometryMaskSubsystemWeak;
};
