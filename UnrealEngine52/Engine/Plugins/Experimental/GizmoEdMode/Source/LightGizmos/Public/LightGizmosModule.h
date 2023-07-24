// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class ILevelEditor;

class FLightGizmosModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void OnLevelEditorCreated(TSharedPtr<ILevelEditor> InLevelEditor);

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Identifiers for the different light gizmo types*/
	static FString PointLightGizmoType;
	static FString SpotLightGizmoType;
	static FString ScalableConeGizmoType;
	static FString DirectionalLightGizmoType;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#endif
