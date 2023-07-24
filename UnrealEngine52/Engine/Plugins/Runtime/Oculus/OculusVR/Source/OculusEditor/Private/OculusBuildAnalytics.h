// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILauncherServicesModule.h"
#include "ILauncher.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "AndroidRuntimeSettings.h"
#include "OculusPluginWrapper.h"

enum EBuildStage
{
	UNDEFINED_STAGE,
	COOK_IN_EDITOR_STAGE,
	COOK_STAGE,
	LAUNCH_UAT_STAGE,
	COMPILE_STAGE,
	PACKAGE_STAGE,
	DEPLOY_STAGE,
	RUN_STAGE,
};

class FOculusBuildAnalytics
{
public:
	static FOculusBuildAnalytics* GetInstance();
	static void Shutdown();
	static bool IsOculusHMDAvailable();

	void RegisterLauncherCallback();
	void OnTelemetryToggled(bool Enabled);

	void OnLauncherCreated(ILauncherRef Launcher);
	void OnLauncherWorkerStarted(ILauncherWorkerPtr LauncherWorker, ILauncherProfileRef Profile);
	void OnStageCompleted(const FString& StageName, double Time);
	void OnStageStarted(const FString& StageName);
	void OnBuildOutputRecieved(const FString& Message);
	void OnCompleted(bool Succeeded, double TotalTime, int32 ErrorCode);
	void SendBuildCompleteEvent(float TotalTime);

private:
	FOculusBuildAnalytics();

	static FOculusBuildAnalytics* instance;

	FDelegateHandle LauncherCallbackHandle;

	float TotalBuildTime;
	float AndroidPackageTime;
	bool BuildCompleted;
	bool UATLaunched;
	int UserAssetCount;
	int BuildStepCount;
	int32 SourceFileCount;
	int64 SourceFileDirectorySize;

	EBuildStage CurrentBuildStage;
	FString CurrentBuildPlatform;
	FString OutputDirectory;
};
