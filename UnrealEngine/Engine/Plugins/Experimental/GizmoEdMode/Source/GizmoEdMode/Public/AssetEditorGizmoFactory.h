// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UEdMode.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "UnrealWidgetFwd.h"

#include "AssetEditorGizmoFactory.generated.h"

class UInteractiveGizmo;
class UInteractiveGizmoManager;

UENUM()
enum class EAssetEditorGizmoFactoryPriority
{
	Default,
	Normal,
	High
};

UINTERFACE()
class GIZMOEDMODE_API UAssetEditorGizmoFactory : public UInterface
{
	GENERATED_BODY()
};

class GIZMOEDMODE_API IAssetEditorGizmoFactory
{
	GENERATED_BODY()
public:
	virtual bool CanBuildGizmoForSelection(FEditorModeTools* ModeTools) const = 0;
	virtual TArray<UInteractiveGizmo*> BuildGizmoForSelection(FEditorModeTools* ModeTools, UInteractiveGizmoManager* GizmoManager) const = 0;
	virtual EAssetEditorGizmoFactoryPriority GetPriority() const { return EAssetEditorGizmoFactoryPriority::Normal; }
	virtual void ConfigureGridSnapping(bool bGridEnabled, bool bRotGridEnabled, const TArray<UInteractiveGizmo*>& Gizmos) const = 0;
};
