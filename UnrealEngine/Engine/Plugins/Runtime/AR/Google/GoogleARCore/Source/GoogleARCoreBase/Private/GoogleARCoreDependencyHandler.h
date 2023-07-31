// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARDependencyHandler.h"

#include "GoogleARCoreDependencyHandler.generated.h"


UCLASS()
class GOOGLEARCOREBASE_API UGoogleARCoreDependencyHandler : public UARDependencyHandler
{
	GENERATED_BODY()
	
public:
	void StartARSessionLatent(UObject* WorldContextObject, UARSessionConfig* SessionConfig, FLatentActionInfo LatentInfo) override;
	void CheckARServiceAvailability(UObject* WorldContextObject, FLatentActionInfo LatentInfo, EARServiceAvailability& OutAvailability) override;
	void InstallARService(UObject* WorldContextObject, FLatentActionInfo LatentInfo, EARServiceInstallRequestResult& OutInstallResult) override;
	void RequestARSessionPermission(UObject* WorldContextObject, UARSessionConfig* SessionConfig, FLatentActionInfo LatentInfo, EARServicePermissionRequestResult& OutPermissionResult) override;
};
