// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ProjectPackagingSettings.h"
#include "PlatformsMenuSettings.generated.h"



UCLASS(config=Game)
class DEVELOPERTOOLSETTINGS_API UPlatformsMenuSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()
	
	/** The directory to which the packaged project will be copied. */
	UPROPERTY(config, EditAnywhere, Category=Project)
	FDirectoryPath StagingDirectory;

	/** Name of the target to use for LaunchOn (only Game/Client targets) */
	UPROPERTY(config)
	FString LaunchOnTarget;

	/** Gets the current launch on target, checking that it's valid, and the default build target if it is not */
	const FTargetInfo* GetLaunchOnTargetInfo() const;
	
	/**
	 * Get and set the per-platform build config and targetplatform settings for the Turnkey/Launch on menu
	 */
	EProjectPackagingBuildConfigurations GetBuildConfigurationForPlatform(FName PlatformName) const;
	void SetBuildConfigurationForPlatform(FName PlatformName, EProjectPackagingBuildConfigurations Configuration);

	FName GetTargetFlavorForPlatform(FName PlatformName) const;
	void SetTargetFlavorForPlatform(FName PlatformName, FName TargetFlavorName);

	FString GetBuildTargetForPlatform(FName PlatformName) const;
	void SetBuildTargetForPlatform(FName PlatformName, FString BuildTargetName);

	const FTargetInfo* GetBuildTargetInfoForPlatform(FName PlatformName, bool& bOutIsProjectTarget) const;

private:
	/** Per platform build configuration */
	UPROPERTY(config)
	TMap<FName, EProjectPackagingBuildConfigurations> PerPlatformBuildConfig;

	/** Per platform flavor cooking target */
	UPROPERTY(config)
	TMap<FName, FName> PerPlatformTargetFlavorName;

	/** Per platform build target */
	UPROPERTY(config, EditAnywhere, Category=Project)
	TMap<FName, FString> PerPlatformBuildTarget;


	
};

