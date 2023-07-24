// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "AssetEditorGizmoFactory.generated.h"

class FEditorModeTools;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Tools/UEdMode.h"
#include "UnrealWidgetFwd.h"
#endif
