// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "AvaInteractiveToolsSettings.generated.h"

UENUM()
enum class EAvaInteractiveToolsDefaultActionAlignment
{
	Axis,
	Camera
};

UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Interactive Tools"))
class AVALANCHEINTERACTIVETOOLS_API UAvaInteractiveToolsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAvaInteractiveToolsSettings();

	/** Distance from the camera at which actors are created. */
	UPROPERTY(Config, EditAnywhere, Category = "Motion Design")
	float CameraDistance = 500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Motion Design")
	EAvaInteractiveToolsDefaultActionAlignment DefaultActionActorAlignment = EAvaInteractiveToolsDefaultActionAlignment::Axis;
};
