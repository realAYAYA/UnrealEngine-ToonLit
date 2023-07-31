// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetEditorGizmoFactory.h"

#include "DirectionalLightGizmoFactory.generated.h"

UCLASS()
class LIGHTGIZMOS_API UDirectionalLightGizmoFactory : public UObject, public IAssetEditorGizmoFactory
{
	GENERATED_BODY()
public:
	//IAssetEditorGizmoFactory interface
	virtual bool CanBuildGizmoForSelection(FEditorModeTools* ModeTools) const override;
	virtual TArray<UInteractiveGizmo*> BuildGizmoForSelection(FEditorModeTools* ModeTools, UInteractiveGizmoManager* GizmoManager) const override;
	virtual EAssetEditorGizmoFactoryPriority GetPriority() const override { return EAssetEditorGizmoFactoryPriority::Normal; }
	virtual void ConfigureGridSnapping(bool bGridEnabled, bool bRotGridEnabled, const TArray<UInteractiveGizmo*> &Gizmo) const override;
};
