// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureOptionalContentInstaller.h"

#include "Algo/AllOf.h"
#include "GameFeaturePluginOperationResult.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureTypes.h"
#include "Logging/StructuredLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureOptionalContentInstaller)

namespace GameFeatureOptionalContentInstaller
{
	static const ELogVerbosity::Type InstallBundleManagerVerbosityOverride = ELogVerbosity::Verbose;

	static const FStringView ErrorNamespace = TEXTVIEW("GameFeaturePlugin.OptionalDownload.");

	static TAutoConsoleVariable<bool> CVarEnableOptionalContentInstaller(TEXT("GameFeatureOptionalContentInstaller.Enable"), 
		true,
		TEXT("Enable optional content installer"));
}

TMulticastDelegate<void(const FString& PluginName, const UE::GameFeatures::FResult&)> UGameFeatureOptionalContentInstaller::OnOptionalContentInstalled;

void UGameFeatureOptionalContentInstaller::BeginDestroy()
{
	IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(CVarSinkHandle);
	Super::BeginDestroy();
}

void UGameFeatureOptionalContentInstaller::Init(TUniqueFunction<TArray<FName>(FString)> InGetOptionalBundlePredicate)
{
	GetOptionalBundlePredicate = MoveTemp(InGetOptionalBundlePredicate);
	BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();

	// Create the cvar sink
	CVarSinkHandle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(
		FConsoleCommandDelegate::CreateUObject(this, &UGameFeatureOptionalContentInstaller::OnCVarsChanged));
	bEnabledCVar = GameFeatureOptionalContentInstaller::CVarEnableOptionalContentInstaller.GetValueOnGameThread();
}

void UGameFeatureOptionalContentInstaller::Enable(bool bInEnable)
{
	bool bOldEnabled = IsEnabled();
	bEnabled = bInEnable;
	bEnabledCVar = GameFeatureOptionalContentInstaller::CVarEnableOptionalContentInstaller.GetValueOnGameThread();
	bool bNewEnabled = IsEnabled();

	if (bOldEnabled != bNewEnabled)
	{
		if (bNewEnabled)
		{
			OnEnabled();
		}
		else
		{
			OnDisabled();
		}
	}
}

void UGameFeatureOptionalContentInstaller::EnableCellularDownloading(bool bEnable)
{
	if (bAllowCellDownload == bEnable)
	{
		return;
	}

	bAllowCellDownload = bEnable;

	// Update flags on active requests
	for ( TPair<FString, FGFPInstall>& Pair : ActiveGFPInstalls)
	{
		BundleManager->UpdateContentRequestFlags(Pair.Value.BundlesEnqueued,
			bEnable ? EInstallBundleRequestFlags::None : EInstallBundleRequestFlags::CheckForCellularDataUsage,
			bEnable ? EInstallBundleRequestFlags::CheckForCellularDataUsage : EInstallBundleRequestFlags::None);
	}
}

bool UGameFeatureOptionalContentInstaller::UpdateContent(const FString& PluginName, bool bIsPredownload)
{
	TArray<FName> Bundles = GetOptionalBundlePredicate(PluginName);

	bool bIsAvailable = false;
	if (!Bundles.IsEmpty())
	{
		TValueOrError<FInstallBundleCombinedInstallState, EInstallBundleResult> MaybeInstallState = BundleManager->GetInstallStateSynchronous(Bundles, false);
		if (MaybeInstallState.HasValue())
		{
			const FInstallBundleCombinedInstallState& InstallState = MaybeInstallState.GetValue();
			bIsAvailable = Algo::AllOf(Bundles, [&InstallState](FName BundleName) { return InstallState.IndividualBundleStates.Contains(BundleName); });
		}
	}

	if (!bIsAvailable)
	{
		return false;
	}

	EInstallBundleRequestFlags InstallFlags = EInstallBundleRequestFlags::AsyncMount;
	if (bIsPredownload)
	{
		InstallFlags |= EInstallBundleRequestFlags::SkipMount;
	}
	if (!bAllowCellDownload)
	{
		InstallFlags |= EInstallBundleRequestFlags::CheckForCellularDataUsage;
	}

	TValueOrError<FInstallBundleRequestInfo, EInstallBundleResult> MaybeRequest = BundleManager->RequestUpdateContent(
		Bundles, 
		InstallFlags, 
		GameFeatureOptionalContentInstaller::InstallBundleManagerVerbosityOverride);

	if (MaybeRequest.HasError())
	{
		UE_LOGFMT(LogGameFeatures, Error, "Failed to request optional content for GFP {GFP}, Error: {Error}", 
			("GFP", PluginName),
			("Error", LexToString(MaybeRequest.GetError())));

		UE::GameFeatures::FResult ErrorResult = MakeError(FString::Printf(TEXT("%.*s%s"),
			GameFeatureOptionalContentInstaller::ErrorNamespace.Len(), GameFeatureOptionalContentInstaller::ErrorNamespace.GetData(),
			LexToString(MaybeRequest.GetError())));
		OnOptionalContentInstalled.Broadcast(PluginName, ErrorResult);

		return false;
	}

	FInstallBundleRequestInfo& Request =  MaybeRequest.GetValue();
	if (!Request.BundlesEnqueued.IsEmpty())
	{
		FGFPInstall& Pending = ActiveGFPInstalls.FindOrAdd(PluginName);

		if (!Pending.CallbackHandle.IsValid())
		{
			Pending.CallbackHandle = IInstallBundleManager::InstallBundleCompleteDelegate.AddUObject(this,
				&UGameFeatureOptionalContentInstaller::OnContentInstalled, PluginName);
		}

		// This should overwrite any previous pending request info
		Pending.BundlesEnqueued = MoveTemp(Request.BundlesEnqueued);
		Pending.bIsPredownload = bIsPredownload;
	}

	return true;
}

void UGameFeatureOptionalContentInstaller::OnContentInstalled(FInstallBundleRequestResultInfo InResult, FString PluginName)
{
	FGFPInstall* MaybeInstall = ActiveGFPInstalls.Find(PluginName);
	if (!MaybeInstall)
	{
		return;
	}

	FGFPInstall& GFPInstall = *MaybeInstall;
	if (!GFPInstall.BundlesEnqueued.Contains(InResult.BundleName))
	{
		return;
	}

	GFPInstall.BundlesEnqueued.Remove(InResult.BundleName);

	if (InResult.Result != EInstallBundleResult::OK)
	{
		if (InResult.OptionalErrorCode.IsEmpty())
		{
			UE_LOGFMT(LogGameFeatures, Error, "Failed to install optional bundle {Bundle} for GFP {GFP}, Error: {Error}",
				("Bundle", InResult.BundleName),
				("GFP", PluginName),
				("Error", LexToString(InResult.Result)));
		}
		else
		{
			UE_LOGFMT(LogGameFeatures, Error, "Failed to install optional bundle {Bundle} for GFP {GFP}, Error: {Error}",
				("Bundle", InResult.BundleName),
				("GFP", PluginName),
				("Error", InResult.OptionalErrorCode));
		}

		//Use OptionalErrorCode and/or OptionalErrorText if available
		const FString ErrorCodeEnding = (InResult.OptionalErrorCode.IsEmpty()) ? LexToString(InResult.Result) : InResult.OptionalErrorCode;
		FText ErrorText = InResult.OptionalErrorCode.IsEmpty() ? UE::GameFeatures::CommonErrorCodes::GetErrorTextForBundleResult(InResult.Result) : InResult.OptionalErrorText;
		UE::GameFeatures::FResult ErrorResult = UE::GameFeatures::FResult(
			MakeError(FString::Printf(TEXT("%.*s%s"), GameFeatureOptionalContentInstaller::ErrorNamespace.Len(), GameFeatureOptionalContentInstaller::ErrorNamespace.GetData(), *ErrorCodeEnding)),
			MoveTemp(ErrorText)
		);
		OnOptionalContentInstalled.Broadcast(PluginName, ErrorResult);

		// Cancel any remaining downloads
		BundleManager->CancelUpdateContent(GFPInstall.BundlesEnqueued);
	}

	if (GFPInstall.BundlesEnqueued.IsEmpty())
	{
		if (GFPInstall.bIsPredownload)
		{
			// Predownload shouldn't pin any cached bundles so release them now

			// Delay call to ReleaseBundlesIfPossible. We don't want to release them from within the complete callback.
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, 
			[this, PluginName, bInstalled = InResult.bContentWasInstalled](float)
			{
				// A machine is active, don't release
				if (!RelevantGFPs.Contains(PluginName))
				{
					ReleaseContent(PluginName);
				}

				if (bInstalled)
				{
					OnOptionalContentInstalled.Broadcast(PluginName, MakeValue());
				}

				return false;
			}));
		}
		else if (InResult.bContentWasInstalled)
		{
			OnOptionalContentInstalled.Broadcast(PluginName, MakeValue());
		}

		// book keeping
		IInstallBundleManager::InstallBundleCompleteDelegate.Remove(GFPInstall.CallbackHandle);
		ActiveGFPInstalls.Remove(PluginName);
	}
}

void UGameFeatureOptionalContentInstaller::ReleaseContent(const FString& PluginName)
{
	TArray<FName> Bundles = GetOptionalBundlePredicate(PluginName);
	if (Bundles.IsEmpty())
	{
		return;
	}

	BundleManager->RequestReleaseContent(
		Bundles, 
		EInstallBundleReleaseRequestFlags::None, 
		{}, 
		GameFeatureOptionalContentInstaller::InstallBundleManagerVerbosityOverride);
}

void UGameFeatureOptionalContentInstaller::OnEnabled()
{
	ensure(RelevantGFPs.IsEmpty());
	RelevantGFPs.Empty();

	UGameFeaturesSubsystem::Get().ForEachGameFeature([this](FGameFeatureInfo&& Info) -> void
	{
		if (UGameFeaturesSubsystem::GetPluginURLProtocol(Info.URL) == EGameFeaturePluginProtocol::InstallBundle &&
			Info.CurrentState >= EGameFeaturePluginState::Downloading)
		{
			if (UpdateContent(Info.Name, false))
			{
				RelevantGFPs.Add(Info.Name);
			}
		}
	});
}

void UGameFeatureOptionalContentInstaller::OnDisabled()
{
	for (const FString& GFP : RelevantGFPs)
	{
		ReleaseContent(GFP);
	}

	RelevantGFPs.Empty();
}

bool UGameFeatureOptionalContentInstaller::IsEnabled() const
{
	return bEnabled && bEnabledCVar;
}

void UGameFeatureOptionalContentInstaller::OnCVarsChanged()
{
	Enable(bEnabled); // Check if CVar changed IsEnabled() and if so, call callbacks
}

void UGameFeatureOptionalContentInstaller::OnGameFeaturePredownloading(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	if (!IsEnabled())
	{
		return;
	}

	UpdateContent(PluginName, true);
	// Predownloads are not 'relevant', they don't have an active state machine
}

void UGameFeatureOptionalContentInstaller::OnGameFeatureDownloading(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	if (!IsEnabled())
	{
		return;
	}

	if (UpdateContent(PluginName, false))
	{
		RelevantGFPs.Add(PluginName);
	}
}

void UGameFeatureOptionalContentInstaller::OnGameFeatureReleasing(const FString& PluginName, const FGameFeaturePluginIdentifier& PluginIdentifier)
{
	if (!IsEnabled())
	{
		return;
	}

	ReleaseContent(PluginName);

	RelevantGFPs.Remove(PluginName);
}
