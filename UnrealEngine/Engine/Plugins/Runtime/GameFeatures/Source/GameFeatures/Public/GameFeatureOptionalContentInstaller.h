// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFeatureStateChangeObserver.h"
#include "HAL/IConsoleManager.h"
#include "InstallBundleManagerInterface.h"
#include "GameFeatureOptionalContentInstaller.generated.h"

namespace UE::GameFeatures
{
	struct FResult;
}

/** 
 * Utilty class to install GFP optional paks (usually containing optional mips) in sync with GFP content installs.
 * NOTE: This only currently supports LRU cached install bundles. It would need UI callbacks and additional support 
 * for free space checks and progress tracking to fully support non-LRU GFPs.
 */
UCLASS(MinimalAPI)
class UGameFeatureOptionalContentInstaller : public UObject, public IGameFeatureStateChangeObserver
{
	GENERATED_BODY()

public:
	static GAMEFEATURES_API TMulticastDelegate<void(const FString& PluginName, const UE::GameFeatures::FResult&)> OnOptionalContentInstalled;

public:
	virtual void BeginDestroy() override;

	void GAMEFEATURES_API Init(TUniqueFunction<TArray<FName>(FString)> GetOptionalBundlePredicate);

	void GAMEFEATURES_API Enable(bool bEnable);

	void GAMEFEATURES_API EnableCellularDownloading(bool bEnable);

private:
	bool UpdateContent(const FString& PluginName, bool bIsPredownload);

	void OnContentInstalled(FInstallBundleRequestResultInfo InResult, FString PluginName);

	void ReleaseContent(const FString& PluginName);

	void OnEnabled();
	void OnDisabled();

	bool IsEnabled() const;

	void OnCVarsChanged();

	// IGameFeatureStateChangeObserver Interface
	virtual void OnGameFeaturePredownloading(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier) override;
	virtual void OnGameFeatureDownloading(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier) override;
	virtual void OnGameFeatureReleasing(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier) override;

private:
	struct FGFPInstall
	{
		FDelegateHandle CallbackHandle;
		TArray<FName> BundlesEnqueued;
		bool bIsPredownload = false;
	};

private:
	TUniqueFunction<TArray<FName>(FString)> GetOptionalBundlePredicate;
	TSharedPtr<IInstallBundleManager> BundleManager;
	TSet<FString> RelevantGFPs;
	TMap<FString, FGFPInstall> ActiveGFPInstalls;

	/** Delegate handle for a console variable sink */
	FConsoleVariableSinkHandle CVarSinkHandle;

	bool bEnabled = false;
	bool bEnabledCVar = false;
	bool bAllowCellDownload = false;
};
