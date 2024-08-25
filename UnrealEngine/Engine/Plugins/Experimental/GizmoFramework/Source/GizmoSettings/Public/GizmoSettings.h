// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EditorGizmos/TransformGizmo.h"

#include "GizmoSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta = (DisplayName = "New TRS Gizmo"))
class GIZMOSETTINGS_API UGizmoSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:

	/** Enable/Disable New TRS Gizmos across the editor. */
	UPROPERTY(config, EditAnywhere, Category = "New TRS Gizmo")
	bool bEnableNewGizmos = false;

	/** Change the current gizmos parameters. */
	UPROPERTY(config, EditAnywhere, Category = "New TRS Gizmo")
	FGizmosParameters GizmoParameters;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUpdateSettings,  const UGizmoSettings*);
	static FOnUpdateSettings OnSettingsChange;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};