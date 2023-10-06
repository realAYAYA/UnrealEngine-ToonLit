// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// TODO: Use UDeveloperSettings + Project Settings UI in editor
// Ideally, we'd use CDO UXRScribeDeveloperSettings to fetch our dev settings
// however, we have some issues now with plugin loading + uobject setup
// Even if we 'fix' those issues, reading the config into the class isn't stood up this early

//#include "Engine/DeveloperSettings.h"
//#include "XRScribeDeveloperSettings.generated.h"
//
///** Developer settings for XRScribe */
//UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "XRScribe"))
//class XRSCRIBE_API UXRScribeDeveloperSettings : public UDeveloperSettings
//{
//	GENERATED_BODY()
//public:
//
//	UXRScribeDeveloperSettings(const FObjectInitializer& Initializer);
//
//	/** Controls whether XRScribe runs in capture or emulation mode. Currently needed at engine startup, but will be runtime switchable. */
//	UPROPERTY(config, EditAnywhere, Category = "XRScribe")
//	EXRScribeRunMode RunMode = EXRScribeRunMode::Capture;
//
//	// TODO:
//	// File path for capture file
//	// Customizing capture dump point (session end, instance teardown, app end)
//	// other run modes (e.g. replay)
//};