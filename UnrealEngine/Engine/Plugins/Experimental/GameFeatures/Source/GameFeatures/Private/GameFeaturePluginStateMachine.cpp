// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeaturePluginStateMachine.h"

#include "AssetRegistry/AssetRegistryState.h"
#include "Components/GameFrameworkComponentManager.h"
#include "GameFeatureData.h"
#include "GameplayTagsManager.h"
#include "Engine/AssetManager.h"
#include "IPlatformFilePak.h"
#include "InstallBundleManagerInterface.h"
#include "InstallBundleUtils.h"
#include "BundlePrereqCombinedStatusHelper.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Algo/AllOf.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/MemoryReader.h"
#include "Stats/Stats.h"
#include "EngineUtils.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeaturesProjectPolicies.h"
#include "Containers/Ticker.h"
#include "UObject/ReferenceChainSearch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeaturePluginStateMachine)

#if WITH_EDITOR
#include "PluginUtils.h"
#endif //if WITH_EDITOR

namespace UE::GameFeatures
{
	static const FString StateMachineErrorNamespace(TEXT("GameFeaturePlugin.StateMachine."));

	static UE::GameFeatures::FResult CanceledResult = MakeError(StateMachineErrorNamespace + TEXT("Canceled"));

	static int32 ShouldLogMountedFiles = 0;
	static FAutoConsoleVariableRef CVarShouldLogMountedFiles(TEXT("GameFeaturePlugin.ShouldLogMountedFiles"),
		ShouldLogMountedFiles,
		TEXT("Should the newly mounted files be logged."));

	static TAutoConsoleVariable<bool> CVarVerifyPluginUnload(TEXT("GameFeaturePlugin.VerifyUnload"), 
		true,
		TEXT("Verify plugin assets are no longer in memory when unloading."),
		ECVF_Default);

	static TAutoConsoleVariable<bool> CVarVerifyPluginUnloadDumpChains(TEXT("GameFeaturePlugin.VerifyUnloadDumpChains"),
		false,
		TEXT("Dump reference chains for any detected plugin asset leaks."),
		ECVF_Default);
	#define GAME_FEATURE_PLUGIN_STATE_TO_STRING(inEnum, inText) case EGameFeaturePluginState::inEnum: return TEXT(#inEnum);
	FString ToString(EGameFeaturePluginState InType)
	{
		switch (InType)
		{
		GAME_FEATURE_PLUGIN_STATE_LIST(GAME_FEATURE_PLUGIN_STATE_TO_STRING)
		default:
			check(0);
			return FString();
		}
	}
	#undef GAME_FEATURE_PLUGIN_STATE_TO_STRING

	// Verify that all assets from this plugin have been unloaded and GC'd	
	void VerifyAssetsUnloaded(const FString& PluginName, bool bIgnoreGameFeatureData)
	{
#if (!UE_BUILD_SHIPPING || UE_SERVER || WITH_EDITOR)
		if (!UE::GameFeatures::CVarVerifyPluginUnload.GetValueOnGameThread())
		{
			return;
		}

		FARFilter PluginArFilter;
		PluginArFilter.PackagePaths.Add(FName(TEXT("/") + PluginName));
		PluginArFilter.bRecursivePaths = true;

		auto CheckForLoadedAsset = [](const FString& PluginName, const FAssetData &AssetData)
		{
			if (AssetData.IsAssetLoaded())
			{
				UE_LOG(LogGameFeatures, Error, TEXT("GFP %s failed to unload asset %s!"), *PluginName, *AssetData.GetFullName());

				if (CVarVerifyPluginUnloadDumpChains.GetValueOnGameThread())
				{
					UObject* AssetObj = AssetData.GetAsset();
					//EInternalObjectFlags InternalFlags = AssetObj->GetInternalFlags();
					FReferenceChainSearch(AssetObj, EReferenceChainSearchMode::Shortest | EReferenceChainSearchMode::PrintResults);
				}

				ensureAlwaysMsgf(false, TEXT("GFP %s failed to unload asset %s!"), *PluginName, *AssetData.GetFullName());
			}
		};

		if (bIgnoreGameFeatureData)
		{
			FARFilter RawGameFeatureDataFilter;
			RawGameFeatureDataFilter.ClassPaths.Add(UGameFeatureData::StaticClass()->GetClassPathName());
			RawGameFeatureDataFilter.bRecursiveClasses = true;

			FARCompiledFilter GameFeatureDataFilter;
			UAssetManager::Get().GetAssetRegistry().CompileFilter(RawGameFeatureDataFilter, GameFeatureDataFilter);

			UAssetManager::Get().GetAssetRegistry().EnumerateAssets(PluginArFilter, [&PluginName, &GameFeatureDataFilter, CheckForLoadedAsset](const FAssetData& AssetData)
				{
					if (UAssetManager::Get().GetAssetRegistry().IsAssetIncludedByFilter(AssetData, GameFeatureDataFilter))
					{
						return true;
					}

					CheckForLoadedAsset(PluginName, AssetData);

					return true;
				});
		}
		else
		{
			UAssetManager::Get().GetAssetRegistry().EnumerateAssets(PluginArFilter, [&PluginName, CheckForLoadedAsset](const FAssetData& AssetData)
				{
					CheckForLoadedAsset(PluginName, AssetData);

					return true;
				});
		}

#endif // UE_BUILD_SHIPPING
	}

#define GAME_FEATURE_PLUGIN_PROTOCOL_PREFIX(inEnum, inString) case EGameFeaturePluginProtocol::inEnum: return inString;
	const TCHAR* GameFeaturePluginProtocolPrefix(EGameFeaturePluginProtocol Protocol)
	{
		switch (Protocol)
		{
			GAME_FEATURE_PLUGIN_PROTOCOL_LIST(GAME_FEATURE_PLUGIN_PROTOCOL_PREFIX)
		}

		check(false);
		return nullptr;
	}
#undef GAME_FEATURE_PLUGIN_PROTOCOL_PREFIX

	namespace CommonErrorCodes
	{
		const FText Generic_FatalError = NSLOCTEXT("GameFeatures", "ErrorCodes.GenericFatalError", "A fatal error has occurred installing the game feature. An update to the application may be needed. Please check for updates and restart the application.");
		const FText Generic_ConnectionError = NSLOCTEXT("GameFeatures", "ErrorCodes.ConnectionGenericError", "An internet connection error has occurred. Please try again later.");
		const FText Generic_MountError = NSLOCTEXT("GameFeatures", "ErrorCodes.MountGenericError", "An error has occurred loading data for this game feature. Please try again later.");

		const FText BundleResult_NeedsUpdate = NSLOCTEXT("GameFeatures", "ErrorCodes.BundleResult.NeedsUpdate", "An application update is required to install this game feature. Please restart the application after downloading any required updates.");
		const FText BundleResult_NeedsDiskSpace = NSLOCTEXT("GameFeatures", "ErrorCodes.BundleResult.NeedsDiskSpace", "You do not have enough disk space to install this game feature. Please try again after clearing up disk space.");
		const FText BundleResult_DownloadCancelled = NSLOCTEXT("GameFeatures", "ErrorCodes.BundleResult.DownloadCancelled", "This game feature download was canceled.");

		const FText ReleaseResult_Generic = NSLOCTEXT("GameFeatures", "ErrorCodes.ReleaseResult.Generic", "There was an error uninstalling the content for this game feature. Please restart the application and try again.");
		const FText ReleaseResult_Cancelled = NSLOCTEXT("GameFeatures", "ErrorCodes.ReleaseResult.Cancelled", "This game feature uninstall was canceled.");

		static const FText& GetErrorTextForBundleResult(EInstallBundleResult ErrorResult)
		{
			switch (ErrorResult)
			{
				//These errors mean an app update is available that we either don't have or failed to get.
				case EInstallBundleResult::FailedPrereqRequiresLatestClient:
				case EInstallBundleResult::FailedPrereqRequiresLatestContent:
				{
					return BundleResult_NeedsUpdate;
				}


				//These are generally unrecoverable and mean something is seriously wrong with the data for this build
				case EInstallBundleResult::InitializationError:
				case EInstallBundleResult::FailedCacheReserve:
				{
					return Generic_FatalError;
				}

				//All of these are indicative of not having enough space to install the required files
				case EInstallBundleResult::InstallerOutOfDiskSpaceError:
				case EInstallBundleResult::ManifestArchiveError:
				{
					return BundleResult_NeedsDiskSpace;
				}

				case EInstallBundleResult::UserCancelledError:
				{
					return BundleResult_DownloadCancelled;
				}

				//Intentionally just show generic error for all these cases
				case EInstallBundleResult::InstallError:
				case EInstallBundleResult::ConnectivityError:
				case EInstallBundleResult::InitializationPending:
				{
					return Generic_ConnectionError;
				}

				//Show generic error for anything missing but log an error
				default:
				{
					UE_LOG(LogGameFeatures, Error, TEXT("Missing error text for EInstallBundleResult %s"), *LexToString(ErrorResult));
					return Generic_ConnectionError;
				}
			}
		}

		static const FText& GetErrorTextForReleaseResult(EInstallBundleReleaseResult ErrorResult)
		{
			switch (ErrorResult)
			{
				case (EInstallBundleReleaseResult::UserCancelledError):
				{
					return ReleaseResult_Cancelled;
				}
			
				case (EInstallBundleReleaseResult::ManifestArchiveError):
				{
					return ReleaseResult_Generic;
				}

				default:
				{
					//Show generic error for anything missing but log an error
					UE_LOG(LogGameFeatures, Error, TEXT("Missing error text for EInstallBundleReleaseResult %s"), *LexToString(ErrorResult));
					return ReleaseResult_Generic;
				}
			}
		}
	};
}

//Enum describing the options for what data can be inside the InstallBundle protocol URL's metadata
//Used to aid in parsing / creating the URL for InstallBundle protocol GameFeaturePlugins
#define INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_LIST(XOPTION)					\
	XOPTION(Bundles,					TEXT("Bundles"))					\
	XOPTION(Version,					TEXT("V"))							\
	XOPTION(Flags,						TEXT("Flags"))						\
	XOPTION(ReleaseFlags,				TEXT("ReleaseFlags"))						\
	XOPTION(UninstallBeforeTerminate,	TEXT("UninstallBeforeTerminate"))	\
	XOPTION(UserPauseDownload,			TEXT("Paused"))

#define INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_ENUM(inEnum, inString) inEnum,
enum class EGameFeatureInstallBundleProtocolOptions : uint8
{
	INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_LIST(INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_ENUM)
	Count,
};
#undef INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_ENUM

#define INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_LEX_TO_STRING(inEnum, inString) \
    case(EGameFeatureInstallBundleProtocolOptions::inEnum):                 \
    {                                                                       \
        return inString;                                                    \
    }                                                                       

FString LexToString(EGameFeatureInstallBundleProtocolOptions FeatureIn)
{
	switch (FeatureIn)
	{
		INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_LIST(INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_LEX_TO_STRING)
		
		default:
		{
			ensureAlwaysMsgf(false, TEXT("Logic error causing a missing LexToString value for EGameFeatureInstallBundleProtocolOptions:%d"), static_cast<uint8>(FeatureIn));
			return ("ERROR_UNSUPPORTED_ENUM");
		}
	}
}
#undef INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_LEX_TO_STRING
#undef INSTALL_BUNDLE_PROTOCOL_URL_OPTIONS_LIST

void FGameFeaturePluginStateStatus::SetTransition(EGameFeaturePluginState InTransitionToState)
{
	TransitionToState = InTransitionToState;
	TransitionResult.ErrorCode = MakeValue();
	TransitionResult.OptionalErrorText = FText();
}

void FGameFeaturePluginStateStatus::SetTransitionError(EGameFeaturePluginState TransitionToErrorState, UE::GameFeatures::FResult TransitionResultIn)
{
	TransitionToState = TransitionToErrorState;
	if (ensureAlwaysMsgf(TransitionResultIn.HasError(), TEXT("Invalid call to SetTransitionError with an FResult that isn't an error! TransitionToErrorState: %s"), *LexToString(TransitionToErrorState)))
	{
		TransitionResult = MoveTemp(TransitionResultIn);
	}
	else
	{
		//Logic error using a non-error FResult, so just generate a general error to keep the SetTransitionError intent
		TransitionResult = MakeError(TEXT("Invalid_Transition_Error"));
	}
}

EGameFeatureInstallBundleProtocolOptions LexFromString(const FString& StringIn)
{
	if (StringIn.IsEmpty())
	{
		return EGameFeatureInstallBundleProtocolOptions::Count;
	}

	for (uint8 EnumIndex = 0; EnumIndex < static_cast<uint8>(EGameFeatureInstallBundleProtocolOptions::Count); ++EnumIndex)
	{
		EGameFeatureInstallBundleProtocolOptions GameFeatureToCheck = static_cast<EGameFeatureInstallBundleProtocolOptions>(EnumIndex);
		if (StringIn.Equals(LexToString(GameFeatureToCheck)))
		{
			return GameFeatureToCheck;
		}
	}

	return  EGameFeatureInstallBundleProtocolOptions::Count;
}

UE::GameFeatures::FResult FGameFeaturePluginState::GetErrorResult(const FString& ErrorCode, const FText OptionalErrorText/*= FText()*/) const
{
	return GetErrorResult(TEXT(""), ErrorCode, OptionalErrorText);
}

UE::GameFeatures::FResult FGameFeaturePluginState::GetErrorResult(const FString& ErrorNamespaceAddition, const FString& ErrorCode, const FText OptionalErrorText/*= FText()*/) const
{
	const FString StateName = LexToString(UGameFeaturesSubsystem::Get().GetPluginState(StateProperties.PluginIdentifier.GetFullPluginURL()));
	const FString ErrorCodeEnding = ErrorNamespaceAddition.IsEmpty() ? ErrorCode : ErrorNamespaceAddition + ErrorCode;
	const FString CompleteErrorCode = (UE::GameFeatures::StateMachineErrorNamespace + StateName + ErrorCodeEnding);
	return UE::GameFeatures::FResult(MakeError(CompleteErrorCode), OptionalErrorText);
}

UE::GameFeatures::FResult FGameFeaturePluginState::GetErrorResult(const FString& ErrorNamespaceAddition, const EInstallBundleResult ErrorResult) const
{
	UE::GameFeatures::FResult BaseResult = GetErrorResult(ErrorNamespaceAddition, LexToString(ErrorResult));
	BaseResult.OptionalErrorText = UE::GameFeatures::CommonErrorCodes::GetErrorTextForBundleResult(ErrorResult);
	return MoveTemp(BaseResult);
}

UE::GameFeatures::FResult FGameFeaturePluginState::GetErrorResult(const FString& ErrorNamespaceAddition, const EInstallBundleReleaseResult ErrorResult) const
{
	UE::GameFeatures::FResult BaseResult = GetErrorResult(ErrorNamespaceAddition, LexToString(ErrorResult));
	BaseResult.OptionalErrorText = UE::GameFeatures::CommonErrorCodes::GetErrorTextForReleaseResult(ErrorResult);
	return MoveTemp(BaseResult);
}


FGameFeaturePluginState::~FGameFeaturePluginState()
{
	CleanupDeferredUpdateCallbacks();
}

bool FGameFeaturePluginState::TryUpdateURLData(const FString& NewPluginURL)
{
	//Early out without any update if we already fully match the new URL
	if (StateProperties.PluginIdentifier.ExactMatchesURL(NewPluginURL))
	{
		return false;
	}

	//Create copied StateMachineProperties data and try and reset it by parsing the new data
	FGameFeaturePluginIdentifier NewIdentifier = FGameFeaturePluginIdentifier(NewPluginURL);
	FGameFeaturePluginStateMachineProperties NewProperties = StateProperties;
	NewProperties.PluginIdentifier = NewIdentifier;

	if (!NewProperties.ParseURL() || !NewProperties.ValidateURLUpdate(StateProperties))
	{
		return false;
	}

	//Parse was successful so transfer to the newly parsed properties
	StateProperties = MoveTemp(NewProperties);

	return true;
}

FDestinationGameFeaturePluginState* FGameFeaturePluginState::AsDestinationState()
{
	FDestinationGameFeaturePluginState* Ret = nullptr;

	EGameFeaturePluginStateType Type = GetStateType();
	if (Type == EGameFeaturePluginStateType::Destination || Type == EGameFeaturePluginStateType::Error)
	{
		Ret = static_cast<FDestinationGameFeaturePluginState*>(this);
	}

	return Ret;
}

FErrorGameFeaturePluginState* FGameFeaturePluginState::AsErrorState()
{
	FErrorGameFeaturePluginState* Ret = nullptr;

	if (GetStateType() == EGameFeaturePluginStateType::Error)
	{
		Ret = static_cast<FErrorGameFeaturePluginState*>(this);
	}

	return Ret;
}

void FGameFeaturePluginState::UpdateStateMachineDeferred(float Delay /*= 0.0f*/) const
{
	CleanupDeferredUpdateCallbacks();

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float dts) mutable
	{
		// @note Release FGameFeaturePluginState::TickHandle first in case the termination callback triggers a GC and destroys the state machine
		TickHandle.Reset();
		StateProperties.OnRequestUpdateStateMachine.ExecuteIfBound();
		return false;
	}), Delay);
}

void FGameFeaturePluginState::GarbageCollectAndUpdateStateMachineDeferred() const
{
	GEngine->ForceGarbageCollection(true); // Tick Delayed

	CleanupDeferredUpdateCallbacks();
	FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FGameFeaturePluginState::UpdateStateMachineDeferred, 0.0f);
}

void FGameFeaturePluginState::UpdateStateMachineImmediate() const
{
	StateProperties.OnRequestUpdateStateMachine.ExecuteIfBound();
}

void FGameFeaturePluginState::UpdateProgress(float Progress) const
{
	StateProperties.OnFeatureStateProgressUpdate.ExecuteIfBound(Progress);
}

void FGameFeaturePluginState::CleanupDeferredUpdateCallbacks() const
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);
}

bool FGameFeaturePluginState::ShouldVisitUninstallStateBeforeTerminal() const
{
	switch (StateProperties.GetPluginProtocol())
	{
		case (EGameFeaturePluginProtocol::InstallBundle):
		{
			//InstallBundleProtocol's have a MetaData that controlls if they uninstall currently
			return StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>().bUninstallBeforeTerminate;
		}

		//Default behavior is to just Terminate
		default:
		{
			return false;
		}
	}
}

/*
=========================================================
  States
=========================================================
*/

struct FGameFeaturePluginState_Uninitialized : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Uninitialized(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		checkf(false, TEXT("UpdateState can not be called while uninitialized"));
	}
};

struct FGameFeaturePluginState_Terminal : public FDestinationGameFeaturePluginState
{
	FGameFeaturePluginState_Terminal(FGameFeaturePluginStateMachineProperties& InStateProperties) : FDestinationGameFeaturePluginState(InStateProperties) {}

	bool bEnteredTerminalState = false;

	virtual bool TryUpdateURLData(const FString& NewPluginURL) override
	{
		//Should never update our URL during Terminal
		return false;
	}

	virtual void BeginState() override
	{
		checkf(!bEnteredTerminalState, TEXT("Plugin entered terminal state more than once! %s"), *StateProperties.PluginIdentifier.GetFullPluginURL());
		bEnteredTerminalState = true;

		UGameFeaturesSubsystem::Get().OnGameFeatureTerminating(StateProperties.PluginName, StateProperties.PluginIdentifier.GetFullPluginURL());
	}
};

struct FGameFeaturePluginState_UnknownStatus : public FDestinationGameFeaturePluginState
{
	FGameFeaturePluginState_UnknownStatus(FGameFeaturePluginStateMachineProperties& InStateProperties) : FDestinationGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::UnknownStatus)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Terminal);
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::UnknownStatus)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::CheckingStatus);

			UGameFeaturesSubsystem::Get().OnGameFeatureCheckingStatus(StateProperties.PluginIdentifier.GetFullPluginURL());
		}
	}
};

struct FGameFeaturePluginState_CheckingStatus : public FGameFeaturePluginState
{
	FGameFeaturePluginState_CheckingStatus(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	bool bParsedURL = false;
	bool bIsAvailable = false;

	virtual void BeginState() override
	{
		bParsedURL = false;
		bIsAvailable = false;
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (!bParsedURL)
		{
			bParsedURL = StateProperties.ParseURL();
			if (!bParsedURL)
			{
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorCheckingStatus, GetErrorResult(TEXT("Bad_PluginURL")));
				return;
			}
		}

		if (StateProperties.GetPluginProtocol() == EGameFeaturePluginProtocol::File)
		{
			bIsAvailable = FPaths::FileExists(StateProperties.PluginInstalledFilename);
		}
		else if (StateProperties.GetPluginProtocol() == EGameFeaturePluginProtocol::InstallBundle)
		{
			TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
			if (BundleManager == nullptr)
			{
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorCheckingStatus, GetErrorResult(TEXT("BundleManager_Null")));
				return;
			}

			if (BundleManager->GetInitState() == EInstallBundleManagerInitState::Failed)
			{
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorCheckingStatus, GetErrorResult(TEXT("BundleManager_Failed_Init")));
				return;
			}

			if (BundleManager->GetInitState() == EInstallBundleManagerInitState::NotInitialized)
			{
				// Just wait for any pending init
				UpdateStateMachineDeferred(1.0f);
				return;
			}

			const TArray<FName>& InstallBundles = StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>().InstallBundles;

			TValueOrError<FInstallBundleCombinedInstallState, EInstallBundleResult> MaybeInstallState = BundleManager->GetInstallStateSynchronous(InstallBundles, false);
			if (MaybeInstallState.HasError())
			{
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorCheckingStatus, GetErrorResult(TEXT("BundleManager_Failed_GetInstallState")));
				return;
			}

			const FInstallBundleCombinedInstallState& InstallState = MaybeInstallState.GetValue();
			bIsAvailable = Algo::AllOf(InstallBundles, [&InstallState](FName BundleName) { return InstallState.IndividualBundleStates.Contains(BundleName); });
		}
		else
		{
			StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorCheckingStatus, GetErrorResult(TEXT("Unknown_Protocol")));
			return;
		}

		if (!bIsAvailable)
		{
			StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorUnavailable, GetErrorResult(TEXT("Plugin_Unavailable")));
			return;
		}

		UGameFeaturesSubsystem::Get().OnGameFeatureStatusKnown(StateProperties.PluginName, StateProperties.PluginIdentifier.GetFullPluginURL());
		StateStatus.SetTransition(EGameFeaturePluginState::StatusKnown);
	}
};

struct FGameFeaturePluginState_ErrorCheckingStatus : public FErrorGameFeaturePluginState
{
	FGameFeaturePluginState_ErrorCheckingStatus(FGameFeaturePluginStateMachineProperties& InStateProperties) : FErrorGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::ErrorCheckingStatus)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Terminal);
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::ErrorCheckingStatus)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::CheckingStatus);
		}
	}
};

struct FGameFeaturePluginState_ErrorUnavailable : public FErrorGameFeaturePluginState
{
	FGameFeaturePluginState_ErrorUnavailable(FGameFeaturePluginStateMachineProperties& InStateProperties) : FErrorGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::ErrorUnavailable)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Terminal);
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::ErrorUnavailable)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::CheckingStatus);
		}
	}
};

struct FGameFeaturePluginState_StatusKnown : public FDestinationGameFeaturePluginState
{
	FGameFeaturePluginState_StatusKnown(FGameFeaturePluginStateMachineProperties& InStateProperties) : FDestinationGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::StatusKnown)
		{
			if (ShouldVisitUninstallStateBeforeTerminal())
			{
				StateStatus.SetTransition(EGameFeaturePluginState::Uninstalling);	
			}
			else
			{
				StateStatus.SetTransition(EGameFeaturePluginState::Terminal);
			}
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::StatusKnown)
		{
			if (StateProperties.GetPluginProtocol() != EGameFeaturePluginProtocol::File)
			{
				StateStatus.SetTransition(EGameFeaturePluginState::Downloading);
			}
			else
			{
				StateStatus.SetTransition(EGameFeaturePluginState::Installed);
			}
		}
	}
};

struct FGameFeaturePluginState_ErrorManagingData : public FErrorGameFeaturePluginState
{
	FGameFeaturePluginState_ErrorManagingData(FGameFeaturePluginStateMachineProperties& InStateProperties) : FErrorGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::ErrorManagingData)
		{			
			StateStatus.SetTransition(EGameFeaturePluginState::Releasing);
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::ErrorManagingData)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Downloading);
		}
	}
};

struct FGameFeaturePluginState_ErrorUninstalling : public FErrorGameFeaturePluginState
{
	FGameFeaturePluginState_ErrorUninstalling(FGameFeaturePluginStateMachineProperties& InStateProperties) : FErrorGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::ErrorUninstalling)
		{
			if (ShouldVisitUninstallStateBeforeTerminal())
			{
				StateStatus.SetTransition(EGameFeaturePluginState::Uninstalling);
			}
			else
			{
				StateStatus.SetTransition(EGameFeaturePluginState::Terminal);
			}
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::ErrorUninstalling)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::StatusKnown);
		}
	}
};

//Base class for our states that want to request to release data from InstallBundleManager
struct FBaseDataReleaseGameFeaturePluginState : public FGameFeaturePluginState
{
	FBaseDataReleaseGameFeaturePluginState(FGameFeaturePluginStateMachineProperties& InStateProperties) 
		: FGameFeaturePluginState(InStateProperties) 
		, Result(MakeValue())
	{}

	virtual ~FBaseDataReleaseGameFeaturePluginState()
	{}

	UE::GameFeatures::FResult Result;
	bool bWasDeleted = false;
	TArray<FName> PendingBundles;

	void OnContentRemoved(FInstallBundleReleaseRequestResultInfo BundleResult)
	{
		if (!PendingBundles.Contains(BundleResult.BundleName))
		{
			return;
		}

		PendingBundles.Remove(BundleResult.BundleName);

		if (!Result.HasError() && BundleResult.Result != EInstallBundleReleaseResult::OK)
		{
			Result = GetErrorResult(TEXT("BundleManager.OnRemove_Failed."), BundleResult.Result);
		}

		if (PendingBundles.Num() > 0)
		{
			return;
		}

		if (Result.HasValue())
		{
			bWasDeleted = true;
		}

		UpdateStateMachineImmediate();
	}

	virtual void BeginState() override
	{
		BeginRemoveRequest();
	}

	virtual void BeginRemoveRequest()
	{
		CleanUp();

		Result = MakeValue();
		bWasDeleted = false;

		if (!ShouldReleaseContent())
		{
			bWasDeleted = true;
			return;
		}

		TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
		check(BundleManager.IsValid());

		const TArray<FName>& InstallBundles = StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>().InstallBundles;

		EInstallBundleReleaseRequestFlags ReleaseFlags = GetReleaseRequestFlags();
		TValueOrError<FInstallBundleRequestInfo, EInstallBundleResult> MaybeRequestInfo = BundleManager->RequestReleaseContent(InstallBundles, ReleaseFlags);

		if (MaybeRequestInfo.HasError())
		{
			ensureMsgf(false, TEXT("Unable to enqueue uninstall for the PluginURL(%s) because %s"), *StateProperties.PluginIdentifier.GetFullPluginURL(), LexToString(MaybeRequestInfo.GetError()));
			Result = GetErrorResult(TEXT("BundleManager.Begin."), MaybeRequestInfo.GetError());

			return;
		}

		FInstallBundleRequestInfo RequestInfo = MaybeRequestInfo.StealValue();

		if (EnumHasAnyFlags(RequestInfo.InfoFlags, EInstallBundleRequestInfoFlags::SkippedUnknownBundles))
		{
			ensureMsgf(false, TEXT("Unable to enqueue uninstall for the PluginURL(%s) because failed to resolve install bundles!"), *StateProperties.PluginIdentifier.GetFullPluginURL());
			Result = GetErrorResult(TEXT("BundleManager.Begin."), TEXT("Resolve_Failed"), UE::GameFeatures::CommonErrorCodes::ReleaseResult_Generic);

			return;
		}

		if (RequestInfo.BundlesEnqueued.Num() == 0)
		{
			bWasDeleted = true;
		}
		else
		{
			PendingBundles = MoveTemp(RequestInfo.BundlesEnqueued);
			IInstallBundleManager::ReleasedDelegate.AddRaw(this, &FBaseDataReleaseGameFeaturePluginState::OnContentRemoved);
		}
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (!Result.HasValue())
		{
			StateStatus.SetTransitionError(GetFailureTransitionState(), Result);
			return;
		}

		if (!bWasDeleted)
		{
			return;
		}

		StateStatus.SetTransition(GetSuccessTransitionState());
	}

	void CleanUp()
	{
		PendingBundles.Empty();
		IInstallBundleManager::ReleasedDelegate.RemoveAll(this);
	}

	virtual void EndState() override
	{
		CleanUp();
	}

	/** Controls what check is done to determine if this state should run or not */
	virtual bool ShouldReleaseContent() const
	{
		switch (StateProperties.GetPluginProtocol())
		{
			case (EGameFeaturePluginProtocol::InstallBundle):
			{
				return true;
			}

			default:
			{
				return false;
			}
		}
	}
	/** Determine what kind of release request flags we submit */
	virtual EInstallBundleReleaseRequestFlags GetReleaseRequestFlags() const
	{
		return StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>().ReleaseInstallBundleFlags;
	}

	/** Determines what state you transition to in the event of a success or failure to release content */
	virtual EGameFeaturePluginState GetSuccessTransitionState() const = 0;
	virtual EGameFeaturePluginState GetFailureTransitionState() const = 0;
};

struct FGameFeaturePluginState_Uninstalling : public FBaseDataReleaseGameFeaturePluginState
{
	FGameFeaturePluginState_Uninstalling(FGameFeaturePluginStateMachineProperties& InStateProperties)
		: FBaseDataReleaseGameFeaturePluginState(InStateProperties)
	{}

	virtual EGameFeaturePluginState GetSuccessTransitionState() const override
	{
		return EGameFeaturePluginState::Terminal;
	}
	
	virtual EGameFeaturePluginState GetFailureTransitionState() const override
	{
		return EGameFeaturePluginState::ErrorUninstalling;
	}

	//Must be overridden to determine what kind of release request we submit
	virtual EInstallBundleReleaseRequestFlags GetReleaseRequestFlags() const override
	{
		const EInstallBundleReleaseRequestFlags BaseFlags = FBaseDataReleaseGameFeaturePluginState::GetReleaseRequestFlags();
		return (BaseFlags | EInstallBundleReleaseRequestFlags::RemoveFilesIfPossible);
	}

	virtual bool TryUpdateURLData(const FString& NewPluginURL) override
	{
		//Use base functionality to update our metadata
		if (!FGameFeaturePluginState::TryUpdateURLData(NewPluginURL))
		{
			return false;
		}

		//If we are no longer uninstalling before terminate, just exit now as a success immediately
		if (!ShouldVisitUninstallStateBeforeTerminal())
		{
			FBaseDataReleaseGameFeaturePluginState::CleanUp();

			Result = MakeValue();
			bWasDeleted = true;
			UpdateStateMachineImmediate();

			return true;
		}		
		
		//Restart our remove request to handle other changes
		BeginRemoveRequest();

		return true;
	}
};

struct FGameFeaturePluginState_Releasing : public FBaseDataReleaseGameFeaturePluginState
{
	FGameFeaturePluginState_Releasing(FGameFeaturePluginStateMachineProperties& InStateProperties)
		: FBaseDataReleaseGameFeaturePluginState(InStateProperties)
	{}

	virtual EGameFeaturePluginState GetSuccessTransitionState() const override
	{
		return EGameFeaturePluginState::StatusKnown;
	}

	virtual EGameFeaturePluginState GetFailureTransitionState() const override
	{
		return EGameFeaturePluginState::ErrorManagingData;
	}
};

struct FGameFeaturePluginState_Downloading : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Downloading(FGameFeaturePluginStateMachineProperties& InStateProperties)
		: FGameFeaturePluginState(InStateProperties)
		, Result(MakeValue())
	{}

	~FGameFeaturePluginState_Downloading()
	{
		Cleanup();
	}

	UE::GameFeatures::FResult Result;
	bool bPluginDownloaded = false;
	TArray<FName> PendingBundleDownloads;
	TUniquePtr<FInstallBundleCombinedProgressTracker> ProgressTracker;
	FTSTicker::FDelegateHandle ProgressUpdateHandle;
	FDelegateHandle GotContentStateHandle;

	void Cleanup()
	{
		if (ProgressUpdateHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(ProgressUpdateHandle);
			ProgressUpdateHandle.Reset();
		}

		if (GotContentStateHandle.IsValid())
		{
			TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
			if (BundleManager)
			{
				BundleManager->CancelAllGetContentStateRequests(GotContentStateHandle);
			}
			GotContentStateHandle.Reset();
		}

		IInstallBundleManager::InstallBundleCompleteDelegate.RemoveAll(this);
		IInstallBundleManager::PausedBundleDelegate.RemoveAll(this);

		Result = MakeValue();
		bPluginDownloaded = false;
		PendingBundleDownloads.Empty();
		ProgressTracker = nullptr;
	}

	void OnGotContentState(FInstallBundleCombinedContentState BundleContentState)
	{
		GotContentStateHandle.Reset();

		TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();

		if (StateProperties.bTryCancel)
		{
			Result = UE::GameFeatures::CanceledResult;
			UpdateStateMachineImmediate();
			return;
		}
		
		FInstallBundlePluginProtocolMetaData& Metadata = StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>();
		const TArray<FName>& InstallBundles = Metadata.InstallBundles;
		
		//Pull our InstallFlags from the Metadata, but also make sure SkipMount is set as there is a separate mounting step that will re-request this
		//without SkipMount and then mount the data, this allows us to pre-download data without mounting it
		EInstallBundleRequestFlags InstallFlags = Metadata.InstallBundleFlags;
		InstallFlags |= EInstallBundleRequestFlags::SkipMount;

		TValueOrError<FInstallBundleRequestInfo, EInstallBundleResult> MaybeRequestInfo = BundleManager->RequestUpdateContent(InstallBundles, InstallFlags);

		if (MaybeRequestInfo.HasError())
		{
			ensureMsgf(false, TEXT("Unable to enqueue download for the PluginURL(%s) because %s"), *StateProperties.PluginIdentifier.GetFullPluginURL(), LexToString(MaybeRequestInfo.GetError()));
			Result = GetErrorResult(TEXT("BundleManager.GotState."), LexToString(MaybeRequestInfo.GetError()));
			UpdateStateMachineImmediate();
			return;
		}

		FInstallBundleRequestInfo RequestInfo = MaybeRequestInfo.StealValue();

		if (EnumHasAnyFlags(RequestInfo.InfoFlags, EInstallBundleRequestInfoFlags::SkippedUnknownBundles))
		{
			ensureMsgf(false, TEXT("Unable to enqueue download for the PluginURL(%s) because failed to resolve install bundles!"), *StateProperties.PluginIdentifier.GetFullPluginURL());
			Result = GetErrorResult(TEXT("BundleManager.GotState."), TEXT("Resolve_Failed"), UE::GameFeatures::CommonErrorCodes::Generic_ConnectionError);
			
			UpdateStateMachineImmediate();
			return;
		}

		if (RequestInfo.BundlesEnqueued.Num() == 0)
		{
			bPluginDownloaded = true;
			UpdateProgress(1.0f);
			UpdateStateMachineImmediate();
		}
		else
		{
			PendingBundleDownloads = MoveTemp(RequestInfo.BundlesEnqueued);
			IInstallBundleManager::InstallBundleCompleteDelegate.AddRaw(this, &FGameFeaturePluginState_Downloading::OnInstallBundleCompleted);
			IInstallBundleManager::PausedBundleDelegate.AddRaw(this, &FGameFeaturePluginState_Downloading::OnInstallBundlePaused);

			ProgressTracker = MakeUnique<FInstallBundleCombinedProgressTracker>(false);
			ProgressTracker->SetBundlesToTrackFromContentState(BundleContentState, PendingBundleDownloads);

			ProgressUpdateHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateRaw(this, &FGameFeaturePluginState_Downloading::OnUpdateProgress)/*, 0.1f*/);

			//If this setting is flipped then we should immediately request to pause downloads.
			//We still generate the downloads so that we have an accurate PendingBundleDownloads list
			if (Metadata.bUserPauseDownload)
			{
				ChangePauseState(true);
			}
		}
	}

	void OnInstallBundleCompleted(FInstallBundleRequestResultInfo BundleResult)
	{
		if (!PendingBundleDownloads.Contains(BundleResult.BundleName))
		{
			return;
		}

		PendingBundleDownloads.Remove(BundleResult.BundleName);

		if (!Result.HasError() && BundleResult.Result != EInstallBundleResult::OK)
		{
			//Use OptionalErrorCode and/or OptionalErrorText if available
			const FString ErrorCodeEnding = (BundleResult.OptionalErrorCode.IsEmpty()) ? LexToString(BundleResult.Result) : BundleResult.OptionalErrorCode;
			const FText ErrorText = BundleResult.OptionalErrorCode.IsEmpty() ? UE::GameFeatures::CommonErrorCodes::GetErrorTextForBundleResult(BundleResult.Result) : BundleResult.OptionalErrorText;
			
			Result = GetErrorResult(TEXT("BundleManager.OnComplete."), ErrorCodeEnding, ErrorText);
			
			if(BundleResult.Result != EInstallBundleResult::UserCancelledError)
			{
				TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
				BundleManager->CancelUpdateContent(PendingBundleDownloads);
			}
		}

		if (PendingBundleDownloads.Num() > 0)
		{
			return;
		}

		if (Result.HasValue())
		{
			bPluginDownloaded = true;
		}

		OnUpdateProgress(0.0f);

		UpdateStateMachineImmediate();
	}

	bool OnUpdateProgress(float dts)
	{
		if (ProgressTracker)
		{
			ProgressTracker->ForceTick();

			float Progress = ProgressTracker->GetCurrentCombinedProgress().ProgressPercent;
			UpdateProgress(Progress);

			UE_LOG(LogGameFeatures, VeryVerbose, TEXT("Download Progress: %f for PluginURL(%s)"), Progress, *StateProperties.PluginIdentifier.GetFullPluginURL());
		}

		return true;
	}

	void ChangePauseState(bool bPause)
	{
		if (PendingBundleDownloads.Num() > 0)
		{
			TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();

			if (bPause)
			{
				BundleManager->PauseUpdateContent(PendingBundleDownloads);
			}
			else
			{
				BundleManager->ResumeUpdateContent(PendingBundleDownloads);
			}
			BundleManager->RequestPausedBundleCallback();

			//Use same text we use for InstallBundleManager's UserPaused as this was also a user pause
			const TCHAR* PauseReason = InstallBundleUtil::GetInstallBundlePauseReason(EInstallBundlePauseFlags::UserPaused);

			NotifyPauseChange(bPause, PauseReason);
		}
	}

	void OnInstallBundlePaused(FInstallBundlePauseInfo InPauseBundleInfo)
	{
		if (PendingBundleDownloads.Contains(InPauseBundleInfo.BundleName))
		{
			const bool bIsPaused = (InPauseBundleInfo.PauseFlags != EInstallBundlePauseFlags::None);
			const TCHAR* PauseReason = InstallBundleUtil::GetInstallBundlePauseReason(InPauseBundleInfo.PauseFlags);
			
			NotifyPauseChange(bIsPaused, PauseReason);
		}
	}

	void NotifyPauseChange(bool bIsPaused, FString PauseReason)
	{
		FGameFeaturePauseStateChangeContext Context(LexToString(EGameFeaturePluginState::Downloading), PauseReason, bIsPaused);
		UGameFeaturesSubsystem::Get().OnGameFeaturePauseChange(StateProperties.PluginIdentifier.GetFullPluginURL(), StateProperties.PluginName, Context);
	}

	virtual void BeginState() override
	{
		Cleanup();

		check(StateProperties.GetPluginProtocol() == EGameFeaturePluginProtocol::InstallBundle);

		TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
		const TArray<FName>& InstallBundles = StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>().InstallBundles;

		GotContentStateHandle = BundleManager->GetContentState(InstallBundles, EInstallBundleGetContentStateFlags::None, true, FInstallBundleGetContentStateDelegate::CreateRaw(this, &FGameFeaturePluginState_Downloading::OnGotContentState));
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (!Result.HasValue())
		{
			StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorManagingData, Result);
			return;
		}

		if (!bPluginDownloaded)
		{
			return;
		}

		StateStatus.SetTransition(EGameFeaturePluginState::Installed);
	}

	virtual void TryCancelState() override
	{
		if (PendingBundleDownloads.Num() > 0)
		{
			TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
			BundleManager->CancelUpdateContent(PendingBundleDownloads);
		}
	}

	virtual bool TryUpdateURLData(const FString& NewPluginURL) override
	{
		//if we don't have any in-progress downloads the default behavior is all we need
		if (PendingBundleDownloads.Num() == 0)
		{
			return FGameFeaturePluginState::TryUpdateURLData(NewPluginURL);
		}
		
		//Need to update our BundleFlags for any bundles we are downloading
		FInstallBundlePluginProtocolMetaData& OldMetaData = StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>();
		EInstallBundleRequestFlags OldRequestFlags = OldMetaData.InstallBundleFlags;
		const bool OldUserPausedFlag = OldMetaData.bUserPauseDownload;

		if (!FGameFeaturePluginState::TryUpdateURLData(NewPluginURL))
		{
			return false;
		}
		FInstallBundlePluginProtocolMetaData& UpdatedMetaData = StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>();
		
		//Update our InstallBundleRequestFlags
		{
			EInstallBundleRequestFlags UpdatedRequestFlags = OldMetaData.InstallBundleFlags;
						
			EInstallBundleRequestFlags AddFlags = (UpdatedRequestFlags & (~OldRequestFlags));
			EInstallBundleRequestFlags RemoveFlags = ((~UpdatedRequestFlags) & OldRequestFlags);

			if ((AddFlags != EInstallBundleRequestFlags::None) || (RemoveFlags != EInstallBundleRequestFlags::None))
			{
				TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();
				BundleManager->UpdateContentRequestFlags(PendingBundleDownloads, AddFlags, RemoveFlags);
			}
		}

		//Handle pausing or resuming the download if the bUserPauseDownload flag has changed
		{
			if (UpdatedMetaData.bUserPauseDownload != OldUserPausedFlag)
			{
				ChangePauseState(UpdatedMetaData.bUserPauseDownload);
			}
		}

		//Return true at this point even if we don't change anything explicitly for this state as the 
		//default FGameFeaturePluginState returned true above and thus changed things
		return true;
	}

	virtual void EndState() override
	{
		Cleanup();
	}
};

struct FGameFeaturePluginState_Installed : public FDestinationGameFeaturePluginState
{
	FGameFeaturePluginState_Installed(FGameFeaturePluginStateMachineProperties& InStateProperties) : FDestinationGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination > EGameFeaturePluginState::Installed)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Mounting);
		}
		else if (StateProperties.Destination < EGameFeaturePluginState::Installed)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Releasing);
		}
	}
};

struct FGameFeaturePluginState_ErrorMounting : public FErrorGameFeaturePluginState
{
	FGameFeaturePluginState_ErrorMounting(FGameFeaturePluginStateMachineProperties& InStateProperties) : FErrorGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::ErrorMounting)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Unmounting);
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::ErrorMounting)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Mounting);
		}
	}
};

struct FGameFeaturePluginState_ErrorWaitingForDependencies : public FErrorGameFeaturePluginState
{
	FGameFeaturePluginState_ErrorWaitingForDependencies(FGameFeaturePluginStateMachineProperties& InStateProperties) : FErrorGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::ErrorWaitingForDependencies)
		{
			// There is no cleaup state equivalent to EGameFeaturePluginState::WaitingForDependencies so just go back to unmounting
			StateStatus.SetTransition(EGameFeaturePluginState::Unmounting);
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::ErrorWaitingForDependencies)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::WaitingForDependencies);
		}
	}
};

struct FGameFeaturePluginState_ErrorRegistering : public FErrorGameFeaturePluginState
{
	FGameFeaturePluginState_ErrorRegistering(FGameFeaturePluginStateMachineProperties& InStateProperties) : FErrorGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::ErrorRegistering)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Unregistering);
		}
		else if (StateProperties.Destination > EGameFeaturePluginState::ErrorRegistering)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Registering);
		}
	}
};

struct FGameFeaturePluginState_Unmounting : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Unmounting(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	UE::GameFeatures::FResult Result = MakeValue();
	TArray<FName> PendingBundles;
	bool bUnmounted = false;

	void OnContentReleased(FInstallBundleReleaseRequestResultInfo BundleResult)
	{
		if (!PendingBundles.Contains(BundleResult.BundleName))
		{
			return;
		}

		PendingBundles.Remove(BundleResult.BundleName);

		if (!Result.HasError() && BundleResult.Result != EInstallBundleReleaseResult::OK)
		{
			Result = GetErrorResult(TEXT("BundleManager.OnReleased."), BundleResult.Result);
		}

		if (PendingBundles.Num() > 0)
		{
			return;
		}

		if (Result.HasValue())
		{
			bUnmounted = true;
		}

		UpdateStateMachineImmediate();
	}

	virtual void BeginState() override
	{
		Result = MakeValue();
		PendingBundles.Empty();
		bUnmounted = false;

		if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(StateProperties.PluginName); 
			Plugin && Plugin->GetDescriptor().bExplicitlyLoaded)
		{
			// The asset registry listens to FPackageName::OnContentPathDismounted() and 
			// will automatically cleanup the asset registry state we added for this plugin.
			// This will also cause any assets we added to the asset manager to be removed.
			// Scan paths added to the asset manager should have already been cleaned up.
			FText FailureReason;
			if (!IPluginManager::Get().UnmountExplicitlyLoadedPlugin(StateProperties.PluginName, &FailureReason))
			{
				ensureMsgf(false, TEXT("Failed to explicitly unmount the PluginURL(%s) because %s"), *StateProperties.PluginIdentifier.GetFullPluginURL(), *FailureReason.ToString());
				Result = GetErrorResult(TEXT("Plugin_Cannot_Explicitly_Unmount"));
				return;
			}
		}

		if (StateProperties.bAddedPluginToManager)
		{
			verify(IPluginManager::Get().RemoveFromPluginsList(StateProperties.PluginName));
			StateProperties.bAddedPluginToManager = false;
		}

		if (StateProperties.GetPluginProtocol() != EGameFeaturePluginProtocol::InstallBundle)
		{
			bUnmounted = true;
			return;
		}

		TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();

		const TArray<FName>& InstallBundles = StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>().InstallBundles;

		EInstallBundleReleaseRequestFlags ReleaseFlags = EInstallBundleReleaseRequestFlags::None;
		TValueOrError<FInstallBundleRequestInfo, EInstallBundleResult> MaybeRequestInfo = BundleManager->RequestReleaseContent(InstallBundles, ReleaseFlags);

		if (MaybeRequestInfo.HasError())
		{
			ensureMsgf(false, TEXT("Unable to enqueue unmount for the PluginURL(%s) because %s"), *StateProperties.PluginIdentifier.GetFullPluginURL(), LexToString(MaybeRequestInfo.GetError()));
			Result = GetErrorResult(TEXT("BundleManager.Begin."), MaybeRequestInfo.GetError());
			return;
		}

		FInstallBundleRequestInfo RequestInfo = MaybeRequestInfo.StealValue();

		if (EnumHasAnyFlags(RequestInfo.InfoFlags, EInstallBundleRequestInfoFlags::SkippedUnknownBundles))
		{
			ensureMsgf(false, TEXT("Unable to enqueue unmount for the PluginURL(%s) because failed to resolve install bundles!"), *StateProperties.PluginIdentifier.GetFullPluginURL());
			Result = GetErrorResult(TEXT("BundleManager.Begin."), TEXT("Cannot_Resolve"), UE::GameFeatures::CommonErrorCodes::Generic_ConnectionError);
			return;
		}

		if (RequestInfo.BundlesEnqueued.Num() == 0)
		{
			bUnmounted = true;
		}
		else
		{
			PendingBundles = MoveTemp(RequestInfo.BundlesEnqueued);
			IInstallBundleManager::ReleasedDelegate.AddRaw(this, &FGameFeaturePluginState_Unmounting::OnContentReleased);
		}
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (!Result.HasValue())
		{
			StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorMounting, Result);
			return;
		}

		if (!bUnmounted)
		{
			return;
		}

		StateStatus.SetTransition(EGameFeaturePluginState::Installed);
	}

	virtual void EndState() override
	{
		IInstallBundleManager::ReleasedDelegate.RemoveAll(this);
	}
};

struct FGameFeaturePluginState_Mounting : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Mounting(FGameFeaturePluginStateMachineProperties& InStateProperties)
		: FGameFeaturePluginState(InStateProperties)
		, Result(MakeValue())
	{}

	UE::GameFeatures::FResult Result;
	TArray<FName> PendingBundles;
	bool bMounted = false;

	void OnInstallBundleCompleted(FInstallBundleRequestResultInfo BundleResult)
	{
		if (!PendingBundles.Contains(BundleResult.BundleName))
		{
			return;
		}

		PendingBundles.Remove(BundleResult.BundleName);

		if (!Result.HasError() && BundleResult.Result != EInstallBundleResult::OK)
		{
			if (BundleResult.OptionalErrorCode.IsEmpty())
			{
				Result = GetErrorResult(TEXT("BundleManager.OnComplete."), BundleResult.Result);
			}
			else
			{
				Result = GetErrorResult(TEXT("BundleManager.OnComplete."), BundleResult.OptionalErrorCode, BundleResult.OptionalErrorText);
			}
		}

		if (PendingBundles.Num() > 0)
		{
			return;
		}

		if (Result.HasValue())
		{
			bMounted = true;
		}

		UpdateStateMachineImmediate();
	}

	void OnPakFileMounted(const IPakFile& PakFile)
	{
		if (FPakFile* Pak = (FPakFile*)(&PakFile))
		{
			UE_LOG(LogGameFeatures, Display, TEXT("Mounted Pak File for (%s) with following files:"), *StateProperties.PluginIdentifier.GetFullPluginURL());
			TArray<FString> OutFileList;
			Pak->GetPrunedFilenames(OutFileList);
			for (const FString& FileName : OutFileList)
			{
				UE_LOG(LogGameFeatures, Display, TEXT("(%s)"), *FileName);
			}
		}
	}

	virtual void BeginState() override
	{
		Result = MakeValue();
		PendingBundles.Empty();
		bMounted = false;

		if (StateProperties.GetPluginProtocol() != EGameFeaturePluginProtocol::InstallBundle)
		{
			bMounted = true;
			return;
		}
		
		TSharedPtr<IInstallBundleManager> BundleManager = IInstallBundleManager::GetPlatformInstallBundleManager();

		FInstallBundlePluginProtocolMetaData MetaData = StateProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>();
		const TArray<FName>& InstallBundles = MetaData.InstallBundles;
		EInstallBundleRequestFlags InstallFlags = MetaData.InstallBundleFlags;

		// Make bundle manager use verbose log level for most logs.
		// We are already done with downloading, so we don't care about logging too much here unless mounting fails.
		const ELogVerbosity::Type InstallBundleManagerVerbosityOverride = ELogVerbosity::Verbose;
		TValueOrError<FInstallBundleRequestInfo, EInstallBundleResult> MaybeRequestInfo = BundleManager->RequestUpdateContent(InstallBundles, InstallFlags, InstallBundleManagerVerbosityOverride);

		if (MaybeRequestInfo.HasError())
		{
			ensureMsgf(false, TEXT("Unable to enqueue mount for the PluginURL(%s) because %s"), *StateProperties.PluginIdentifier.GetFullPluginURL(), LexToString(MaybeRequestInfo.GetError()));
			Result = GetErrorResult(TEXT("BundleManager.Begin.CannotStart."), MaybeRequestInfo.GetError());
			return;
		}

		FInstallBundleRequestInfo RequestInfo = MaybeRequestInfo.StealValue();

		if (EnumHasAnyFlags(RequestInfo.InfoFlags, EInstallBundleRequestInfoFlags::SkippedUnknownBundles))
		{
			ensureMsgf(false, TEXT("Unable to enqueue mount for the PluginURL(%s) because failed to resolve install bundles!"), *StateProperties.PluginIdentifier.GetFullPluginURL());
			Result = GetErrorResult(TEXT("BundleManager.Begin."), TEXT("Resolve_Failed"));
			return;
		}

		if (RequestInfo.BundlesEnqueued.Num() == 0)
		{
			bMounted = true;
		}
		else
		{
			PendingBundles = MoveTemp(RequestInfo.BundlesEnqueued);
			IInstallBundleManager::InstallBundleCompleteDelegate.AddRaw(this, &FGameFeaturePluginState_Mounting::OnInstallBundleCompleted);
			if (UE::GameFeatures::ShouldLogMountedFiles)
			{
				FCoreDelegates::OnPakFileMounted2.AddRaw(this, &FGameFeaturePluginState_Mounting::OnPakFileMounted);
			}
		}
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (!Result.HasValue())
		{
			StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorMounting, Result);
			return;
		}
		if (!bMounted)
		{
			return;
		}

		checkf(!StateProperties.PluginInstalledFilename.IsEmpty(), TEXT("PluginInstalledFilename must be set by the Mounting. PluginURL: %s"), *StateProperties.PluginIdentifier.GetFullPluginURL());
		checkf(FPaths::GetExtension(StateProperties.PluginInstalledFilename) == TEXT("uplugin"), TEXT("PluginInstalledFilename must have a uplugin extension. PluginURL: %s"), *StateProperties.PluginIdentifier.GetFullPluginURL());

		// refresh the plugins list to let the plugin manager know about it
		const TSharedPtr<IPlugin> MaybePlugin = IPluginManager::Get().FindPlugin(StateProperties.PluginName);
		const bool bNeedsPluginMount = (MaybePlugin == nullptr || MaybePlugin->GetDescriptor().bExplicitlyLoaded);

		if (MaybePlugin)
		{
			if (!FPaths::IsSamePath(MaybePlugin->GetDescriptorFileName(), StateProperties.PluginInstalledFilename))
			{
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorMounting, GetErrorResult(TEXT("Plugin_Name_Already_In_Use")));
				return;
			}
		}
		else
		{
			const bool bAddedPlugin = IPluginManager::Get().AddToPluginsList(StateProperties.PluginInstalledFilename);
			if (!bAddedPlugin)
			{
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorMounting, GetErrorResult(TEXT("Failed_To_Register_Plugin")));
				return;
			}

			StateProperties.bAddedPluginToManager = true;
		}
		
		if (bNeedsPluginMount)
		{
			IPluginManager::Get().MountExplicitlyLoadedPlugin(StateProperties.PluginName);
		}

		// After the new plugin is mounted add the asset registry for that plugin.
		if (StateProperties.GetPluginProtocol() == EGameFeaturePluginProtocol::InstallBundle)
		{
			const FString PluginFolder = FPaths::GetPath(StateProperties.PluginInstalledFilename);
			const FString PluginAssetRegistry = PluginFolder / TEXT("AssetRegistry.bin");

			TSharedPtr<IPlugin> NewlyMountedPlugin = IPluginManager::Get().FindPlugin(StateProperties.PluginName);
			if (NewlyMountedPlugin.IsValid() && NewlyMountedPlugin->CanContainContent() && IFileManager::Get().FileExists(*PluginAssetRegistry))
			{
				TArray<uint8> SerializedAssetData;
				if (!FFileHelper::LoadFileToArray(SerializedAssetData, *PluginAssetRegistry))
				{
					StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorMounting, GetErrorResult(TEXT("Failed_To_Load_Plugin_AssetRegistry")));
					return;
				}

				FAssetRegistryState PluginAssetRegistryState;
				FMemoryReader Ar(SerializedAssetData);
				PluginAssetRegistryState.Load(Ar);

				IAssetRegistry& AssetRegistry = UAssetManager::Get().GetAssetRegistry();
				AssetRegistry.AppendState(PluginAssetRegistryState);
			}
		}

		StateStatus.SetTransition(EGameFeaturePluginState::WaitingForDependencies);
	}

	virtual void EndState() override
	{
		IInstallBundleManager::InstallBundleCompleteDelegate.RemoveAll(this);
		FCoreDelegates::OnPakFileMounted2.RemoveAll(this);
	}
};

struct FGameFeaturePluginState_WaitingForDependencies : public FGameFeaturePluginState
{
	FGameFeaturePluginState_WaitingForDependencies(FGameFeaturePluginStateMachineProperties& InStateProperties)
		: FGameFeaturePluginState(InStateProperties)
		, bRequestedDependencies(false)
	{}

	virtual ~FGameFeaturePluginState_WaitingForDependencies()
	{
		ClearDependencies();
	}

	virtual void BeginState() override
	{
		ClearDependencies();
	}

	virtual void EndState() override
	{
		ClearDependencies();
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		checkf(!StateProperties.PluginInstalledFilename.IsEmpty(), TEXT("PluginInstalledFilename must be set by the loading dependencies phase. PluginURL: %s"), *StateProperties.PluginIdentifier.GetFullPluginURL());
		checkf(FPaths::GetExtension(StateProperties.PluginInstalledFilename) == TEXT("uplugin"), TEXT("PluginInstalledFilename must have a uplugin extension. PluginURL: %s"), *StateProperties.PluginIdentifier.GetFullPluginURL());

		if (!bRequestedDependencies)
		{
			UGameFeaturesSubsystem& GameFeaturesSubsystem = UGameFeaturesSubsystem::Get();

			TArray<UGameFeaturePluginStateMachine*> Dependencies;
			if (!GameFeaturesSubsystem.FindOrCreatePluginDependencyStateMachines(StateProperties.PluginInstalledFilename, Dependencies))
			{
				// Failed to query dependencies
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorWaitingForDependencies, GetErrorResult(TEXT("Failed_Dependency_Query")));
				return;
			}
			
			bRequestedDependencies = true;

			RemainingDependencies.Reserve(Dependencies.Num());
			for (UGameFeaturePluginStateMachine* Dependency : Dependencies)
			{	
				RemainingDependencies.Emplace(Dependency, MakeValue());
				TransitionDependency(Dependency);
			}
		}

		for (FDepResultPair& Pair : RemainingDependencies)
		{
			UGameFeaturePluginStateMachine* RemainingDependency = Pair.Key.Get();
			if (!RemainingDependency)
			{
				// One of the dependency state machines was destroyed before finishing
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorWaitingForDependencies, GetErrorResult(TEXT("Dependency_Destroyed_Before_Finish")));
				return;
			}

			if (Pair.Value.HasError())
			{
				StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorWaitingForDependencies, GetErrorResult(TEXT("Failed_Dependency_Register")));
				return;
			}
		}

		if (RemainingDependencies.Num() == 0)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Registering);
		}
	}

	FGameFeaturePluginStateRange GetDependencyStateRange() const
	{
		return FGameFeaturePluginStateRange(EGameFeaturePluginState::Registered, EGameFeaturePluginState::Active);
	}

	void TransitionDependency(UGameFeaturePluginStateMachine* Dependency)
	{
		const bool bSetDestination = Dependency->SetDestination(GetDependencyStateRange(),
			FGameFeatureStateTransitionComplete::CreateRaw(this, &FGameFeaturePluginState_WaitingForDependencies::OnDependencyTransitionComplete));

		if (!bSetDestination)
		{
			const bool bCancelPending = Dependency->TryCancel(
				FGameFeatureStateTransitionCanceled::CreateRaw(this, &FGameFeaturePluginState_WaitingForDependencies::OnDependencyTransitionCanceled));
			if (!ensure(bCancelPending))
			{
				OnDependencyTransitionComplete(Dependency, GetErrorResult(UE::GameFeatures::CommonErrorCodes::DependencyFailedRegister));
			}
		}
	}

	void OnDependencyTransitionCanceled(UGameFeaturePluginStateMachine* Dependency)
	{
		// Special case for terminal state since it cannot be exited, we need to make a new machine
		if (Dependency->GetCurrentState() == EGameFeaturePluginState::Terminal)
		{
			UGameFeaturePluginStateMachine* NewMachine = UGameFeaturesSubsystem::Get().FindOrCreateGameFeaturePluginStateMachine(Dependency->GetPluginURL());
			checkf(NewMachine != Dependency, TEXT("Game Feature Plugin %s should have already been removed from subsystem!"), *Dependency->GetPluginURL());

			const int32 Index = RemainingDependencies.IndexOfByPredicate([Dependency](const FDepResultPair& Pair)
				{
					return Pair.Key == Dependency;
				});

			check(Index != INDEX_NONE);
			FDepResultPair& FoundDep = RemainingDependencies[Index];
			FoundDep.Key = NewMachine;

			Dependency->RemovePendingTransitionCallback(this);
			Dependency->RemovePendingCancelCallback(this);

			Dependency = NewMachine;
		}

		// Now that the transition has been canceled, retry reaching the desired destination
		const bool bSetDestination = Dependency->SetDestination(GetDependencyStateRange(),
			FGameFeatureStateTransitionComplete::CreateRaw(this, &FGameFeaturePluginState_WaitingForDependencies::OnDependencyTransitionComplete));

		if (!ensure(bSetDestination))
		{
			OnDependencyTransitionComplete(Dependency, GetErrorResult(UE::GameFeatures::CommonErrorCodes::DependencyFailedRegister));
		}
	}

	void OnDependencyTransitionComplete(UGameFeaturePluginStateMachine* Dependency, const UE::GameFeatures::FResult& Result)
	{
		const int32 Index = RemainingDependencies.IndexOfByPredicate([Dependency](const FDepResultPair& Pair)
		{
			return Pair.Key == Dependency;
		});

		if (Index != INDEX_NONE)
		{
			FDepResultPair& FoundDep = RemainingDependencies[Index];
			
			if (Result.HasError())
			{
				FoundDep.Value = Result;
			}
			else
			{
				RemainingDependencies.RemoveAtSwap(Index, 1, false);
			}

			UpdateStateMachineImmediate();
		}
	}

	void ClearDependencies()
	{
		for (FDepResultPair& Pair : RemainingDependencies)
		{
			UGameFeaturePluginStateMachine* RemainingDependency = Pair.Key.Get();
			if (RemainingDependency)
			{
				RemainingDependency->RemovePendingTransitionCallback(this);
				RemainingDependency->RemovePendingCancelCallback(this);
			}
		}

		RemainingDependencies.Empty();
		bRequestedDependencies = false;
	}

	using FDepResultPair = TPair<TWeakObjectPtr<UGameFeaturePluginStateMachine>, UE::GameFeatures::FResult>;
	TArray<FDepResultPair> RemainingDependencies;
	bool bRequestedDependencies = false;
};

struct FGameFeaturePluginState_Unregistering : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Unregistering(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	bool bRequestedGC = false;

	virtual void BeginState()
	{
		bRequestedGC = false;
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (bRequestedGC)
		{
			UE::GameFeatures::VerifyAssetsUnloaded(StateProperties.PluginName, false);

			StateStatus.SetTransition(EGameFeaturePluginState::Unmounting);
			return;
		}

		if (StateProperties.GameFeatureData)
		{
			UGameFeaturesSubsystem::Get().OnGameFeatureUnregistering(StateProperties.GameFeatureData, StateProperties.PluginName, StateProperties.PluginIdentifier.GetFullPluginURL());

			UGameFeaturesSubsystem::Get().RemoveGameFeatureFromAssetManager(StateProperties.GameFeatureData, StateProperties.PluginName, StateProperties.AddedPrimaryAssetTypes);
			StateProperties.AddedPrimaryAssetTypes.Empty();

			UGameFeaturesSubsystem::Get().UnloadGameFeatureData(StateProperties.GameFeatureData);
		}

		StateProperties.GameFeatureData = nullptr;

#if WITH_EDITOR
		// This will properly unload any plugin asset that could be opened in the editor
		// and ensure standalone packages get unloaded as well
		verify(FPluginUtils::UnloadPluginAssets(StateProperties.PluginName));
#endif //if WITH_EDITOR

		// Try to remove the gameplay tags, this might be ignored depending on project settings
		const FString PluginFolder = FPaths::GetPath(StateProperties.PluginInstalledFilename);
		UGameplayTagsManager::Get().RemoveTagIniSearchPath(PluginFolder / TEXT("Config") / TEXT("Tags"));

		bRequestedGC = true;
		GarbageCollectAndUpdateStateMachineDeferred();
	}
};

struct FGameFeaturePluginState_Registering : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Registering(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		const FString PluginFolder = FPaths::GetPath(StateProperties.PluginInstalledFilename);
		UGameplayTagsManager::Get().AddTagIniSearchPath(PluginFolder / TEXT("Config") / TEXT("Tags"));

		const FString BackupGameFeatureDataPath = FString::Printf(TEXT("/%s/%s.%s"), *StateProperties.PluginName, *StateProperties.PluginName, *StateProperties.PluginName);

		FString PreferredGameFeatureDataPath = TEXT("/") + StateProperties.PluginName + TEXT("/GameFeatureData.GameFeatureData");
		// Allow game feature location to be overriden globally and from within the plugin
		FString OverrideIniPathName = StateProperties.PluginName + TEXT("_Override");
		FString OverridePath = GConfig->GetStr(TEXT("GameFeatureData"), *OverrideIniPathName, GGameIni);
		if (OverridePath.IsEmpty())
		{
			const FString SettingsOverride = PluginFolder / TEXT("Config") / TEXT("Settings.ini");
			if (FPaths::FileExists(SettingsOverride))
			{
				GConfig->LoadFile(SettingsOverride);
				OverridePath = GConfig->GetStr(TEXT("GameFeatureData"), TEXT("Override"), SettingsOverride);
				GConfig->UnloadFile(SettingsOverride);
			}
		}
		if (!OverridePath.IsEmpty())
		{
			PreferredGameFeatureDataPath = OverridePath;
		}
		
		auto LoadGameFeatureData = [](const FString& Path) -> UGameFeatureData*
		{
			TSharedPtr<FStreamableHandle> GameFeatureDataHandle;
			if (FPackageName::DoesPackageExist(Path))
			{
				GameFeatureDataHandle = UGameFeaturesSubsystem::LoadGameFeatureData(Path);
				// @todo make this async. For now we just wait
				if (GameFeatureDataHandle.IsValid())
				{
					GameFeatureDataHandle->WaitUntilComplete(0.0f, false);
					return Cast<UGameFeatureData>(GameFeatureDataHandle->GetLoadedAsset());
				}
			}

			return nullptr;
		};

		StateProperties.GameFeatureData = LoadGameFeatureData(PreferredGameFeatureDataPath);
		if (!StateProperties.GameFeatureData)
		{
			StateProperties.GameFeatureData = LoadGameFeatureData(BackupGameFeatureDataPath);
		}

		if (StateProperties.GameFeatureData)
		{
			StateProperties.GameFeatureData->InitializeBasePluginIniFile(StateProperties.PluginInstalledFilename);
			StateStatus.SetTransition(EGameFeaturePluginState::Registered);

			check(StateProperties.AddedPrimaryAssetTypes.Num() == 0);
			UGameFeaturesSubsystem::Get().AddGameFeatureToAssetManager(StateProperties.GameFeatureData, StateProperties.PluginName, StateProperties.AddedPrimaryAssetTypes);

			UGameFeaturesSubsystem::Get().OnGameFeatureRegistering(StateProperties.GameFeatureData, StateProperties.PluginName, StateProperties.PluginIdentifier.GetFullPluginURL());
		}
		else
		{
			// The gamefeaturedata does not exist. The pak file may not be openable or this is a builtin plugin where the pak file does not exist.
			StateStatus.SetTransitionError(EGameFeaturePluginState::ErrorRegistering, GetErrorResult(TEXT("Plugin_Missing_GameFeatureData")));
		}
	}
};

struct FGameFeaturePluginState_Registered : public FDestinationGameFeaturePluginState
{
	FGameFeaturePluginState_Registered(FGameFeaturePluginStateMachineProperties& InStateProperties) : FDestinationGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination > EGameFeaturePluginState::Registered)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Loading);
		}
		else if (StateProperties.Destination < EGameFeaturePluginState::Registered)
		{
			StateStatus.SetTransition( EGameFeaturePluginState::Unregistering);
		}
	}
};

struct FGameFeaturePluginState_Unloading : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Unloading(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	bool bRequestedGC = false;

	virtual void BeginState() 
	{
		bRequestedGC = false;
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (bRequestedGC)
		{
#if !WITH_EDITOR // Disabled in editor since it's likely to report unloaded assets because of standalone packages
			UE::GameFeatures::VerifyAssetsUnloaded(StateProperties.PluginName, true);
#endif //if !WITH_EDITOR

			StateStatus.SetTransition(EGameFeaturePluginState::Registered);
			return;
		}

		UnloadGameFeatureBundles(StateProperties.GameFeatureData);

		if (StateProperties.Destination.MaxState == EGameFeaturePluginState::Registered)
		{
			// If we aren't going farther than Registered, GC now
			// otherwise we will defer until closer to our destination state
			bRequestedGC = true;
			GarbageCollectAndUpdateStateMachineDeferred();
			return;
		}

		StateStatus.SetTransition(EGameFeaturePluginState::Registered);
	}

	void UnloadGameFeatureBundles(const UGameFeatureData* GameFeatureToLoad)
	{
		if (GameFeatureToLoad == nullptr)
		{
			return;
		}

		const UGameFeaturesProjectPolicies& Policy = UGameFeaturesSubsystem::Get().GetPolicy();

		// Remove all bundles from feature data and completely unload everything else
		FPrimaryAssetId GameFeatureAssetId = GameFeatureToLoad->GetPrimaryAssetId();
		TSharedPtr<FStreamableHandle> Handle = UAssetManager::Get().ChangeBundleStateForPrimaryAssets({ GameFeatureAssetId }, {}, {}, /*bRemoveAllBundles=*/ true);
		ensureAlways(Handle == nullptr || Handle->HasLoadCompleted()); // Should be no handle since nothing is being loaded

		TArray<FPrimaryAssetId> AssetIds = Policy.GetPreloadAssetListForGameFeature(GameFeatureToLoad, /*bIncludeLoadedAssets=*/true);

		// Don't unload game feature data asset yet, that will happen in FGameFeaturePluginState_Unregistering
		ensureAlways(AssetIds.RemoveSwap(GameFeatureAssetId, false) == 0);

		if (AssetIds.Num() > 0)
		{
			UAssetManager::Get().UnloadPrimaryAssets(AssetIds);
		}
	}
};

struct FGameFeaturePluginState_Loading : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Loading(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		check(StateProperties.GameFeatureData);

		// AssetManager
		TSharedPtr<FStreamableHandle> BundleHandle = LoadGameFeatureBundles(StateProperties.GameFeatureData);
		// @todo make this async. For now we just wait
		if (BundleHandle.IsValid())
		{
			BundleHandle->WaitUntilComplete(0.0f, false);
		}

		UGameFeaturesSubsystem::Get().OnGameFeatureLoading(StateProperties.GameFeatureData, StateProperties.PluginIdentifier.GetFullPluginURL());

		StateStatus.SetTransition(EGameFeaturePluginState::Loaded);
	}

	/** Loads primary assets and bundles for the specified game feature */
	TSharedPtr<FStreamableHandle> LoadGameFeatureBundles(const UGameFeatureData* GameFeatureToLoad)
	{
		check(GameFeatureToLoad);

		const UGameFeaturesProjectPolicies& Policy = UGameFeaturesSubsystem::Get().GetPolicy<UGameFeaturesProjectPolicies>();

		TArray<FPrimaryAssetId> AssetIdsToLoad = Policy.GetPreloadAssetListForGameFeature(GameFeatureToLoad);

		FPrimaryAssetId GameFeatureAssetId = GameFeatureToLoad->GetPrimaryAssetId();
		if (GameFeatureAssetId.IsValid())
		{
			AssetIdsToLoad.Add(GameFeatureAssetId);
		}

		TSharedPtr<FStreamableHandle> RetHandle;
		if (AssetIdsToLoad.Num() > 0)
		{
			RetHandle = UAssetManager::Get().LoadPrimaryAssets(AssetIdsToLoad, Policy.GetPreloadBundleStateForGameFeature());
		}

		return RetHandle;
	}
};

struct FGameFeaturePluginState_Loaded : public FDestinationGameFeaturePluginState
{
	FGameFeaturePluginState_Loaded(FGameFeaturePluginStateMachineProperties& InStateProperties) : FDestinationGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination > EGameFeaturePluginState::Loaded)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Activating);
		}
		else if (StateProperties.Destination < EGameFeaturePluginState::Loaded)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Unloading);
		}
	}
};

struct FGameFeaturePluginState_Deactivating : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Deactivating(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	int32 NumObservedPausers = 0;
	int32 NumExpectedPausers = 0;
	bool bInProcessOfDeactivating = false;
	bool bRequestedGC = false;

	virtual void BeginState() override
	{
		NumObservedPausers = 0;
		NumExpectedPausers = 0;
		bInProcessOfDeactivating = false;
		bRequestedGC = false;
	}

	void OnPauserCompleted()
	{
		check(IsInGameThread());
		ensure(NumExpectedPausers != INDEX_NONE);
		++NumObservedPausers;

		if (NumObservedPausers == NumExpectedPausers)
		{
			UpdateStateMachineImmediate();
		}
	}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (bRequestedGC)
		{
			check(NumExpectedPausers == NumObservedPausers);
			StateStatus.SetTransition(EGameFeaturePluginState::Loaded);
			return;
		}

		if (!bInProcessOfDeactivating)
		{
			// Make sure we won't complete the transition prematurely if someone registers as a pauser but fires immediately
			bInProcessOfDeactivating = true;
			NumExpectedPausers = INDEX_NONE;
			NumObservedPausers = 0;

			// Deactivate
			FGameFeatureDeactivatingContext Context(FSimpleDelegate::CreateRaw(this, &FGameFeaturePluginState_Deactivating::OnPauserCompleted));
			UGameFeaturesSubsystem::Get().OnGameFeatureDeactivating(StateProperties.GameFeatureData, StateProperties.PluginName, Context, StateProperties.PluginIdentifier.GetFullPluginURL());
			NumExpectedPausers = Context.NumPausers;

			// Since we are pausing work during this deactivation, also notify the OnGameFeaturePauseChange delegate
			if (NumExpectedPausers > 0)
			{
				FGameFeaturePauseStateChangeContext PauseContext(LexToString(EGameFeaturePluginState::Deactivating), TEXT("PendingDeactivationCallbacks"), true);
				UGameFeaturesSubsystem::Get().OnGameFeaturePauseChange(StateProperties.PluginIdentifier.GetFullPluginURL(), StateProperties.PluginName, PauseContext);
			}
		}

		if (NumExpectedPausers == NumObservedPausers)
		{
			//If we previously sent an OnGameFeaturePauseChange delegate we need to send that work is now unpaused
			if (NumExpectedPausers > 0)
			{
				FGameFeaturePauseStateChangeContext PauseContext(LexToString(EGameFeaturePluginState::Deactivating), TEXT(""), false);
				UGameFeaturesSubsystem::Get().OnGameFeaturePauseChange(StateProperties.PluginIdentifier.GetFullPluginURL(), StateProperties.PluginName, PauseContext);
			}

			if (!bRequestedGC && StateProperties.Destination.MaxState == EGameFeaturePluginState::Loaded)
			{
				// If we aren't going farther than Loaded, GC now
				// otherwise we will defer until closer to our destination state
				bRequestedGC = true;
				GarbageCollectAndUpdateStateMachineDeferred();
			}
			else
			{
				StateStatus.SetTransition(EGameFeaturePluginState::Loaded);
			}
		}
		else
		{
			UE_LOG(LogGameFeatures, Log, TEXT("Game feature %s deactivation paused until %d observer tasks complete their deactivation"), *GetPathNameSafe(StateProperties.GameFeatureData), NumExpectedPausers - NumObservedPausers);
		}
	}
};

struct FGameFeaturePluginState_Activating : public FGameFeaturePluginState
{
	FGameFeaturePluginState_Activating(FGameFeaturePluginStateMachineProperties& InStateProperties) : FGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		check(GEngine);
		check(StateProperties.GameFeatureData);

		FGameFeatureActivatingContext Context;

		StateProperties.GameFeatureData->InitializeHierarchicalPluginIniFiles(StateProperties.PluginInstalledFilename);

		UGameFeaturesSubsystem::Get().OnGameFeatureActivating(StateProperties.GameFeatureData, StateProperties.PluginName, Context, StateProperties.PluginIdentifier.GetFullPluginURL());

		StateStatus.SetTransition(EGameFeaturePluginState::Active);
	}
};

struct FGameFeaturePluginState_Active : public FDestinationGameFeaturePluginState
{
	FGameFeaturePluginState_Active(FGameFeaturePluginStateMachineProperties& InStateProperties) : FDestinationGameFeaturePluginState(InStateProperties) {}

	virtual void UpdateState(FGameFeaturePluginStateStatus& StateStatus) override
	{
		if (StateProperties.Destination < EGameFeaturePluginState::Active)
		{
			StateStatus.SetTransition(EGameFeaturePluginState::Deactivating);
		}
	}
};

/*
=========================================================
  State Machine
=========================================================
*/

UGameFeaturePluginStateMachine::UGameFeaturePluginStateMachine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CurrentStateInfo(EGameFeaturePluginState::Uninitialized)
	, bInUpdateStateMachine(false)
{

}

void UGameFeaturePluginStateMachine::InitStateMachine(const FString& InPluginURL)
{
	check(GetCurrentState() == EGameFeaturePluginState::Uninitialized);
	CurrentStateInfo.State = EGameFeaturePluginState::UnknownStatus;
	StateProperties = FGameFeaturePluginStateMachineProperties(
		InPluginURL,
		FGameFeaturePluginStateRange(CurrentStateInfo.State),
		FGameFeaturePluginRequestUpdateStateMachine::CreateUObject(this, &ThisClass::UpdateStateMachine),
		FGameFeatureStateProgressUpdate::CreateUObject(this, &ThisClass::UpdateCurrentStateProgress));

#define GAME_FEATURE_PLUGIN_STATE_MAKE_STATE(inEnum, inText) AllStates[EGameFeaturePluginState::inEnum] = MakeUnique<FGameFeaturePluginState_##inEnum>(StateProperties);
	GAME_FEATURE_PLUGIN_STATE_LIST(GAME_FEATURE_PLUGIN_STATE_MAKE_STATE)
#undef GAME_FEATURE_PLUGIN_STATE_MAKE_STATE

	AllStates[CurrentStateInfo.State]->BeginState();
}

bool UGameFeaturePluginStateMachine::SetDestination(FGameFeaturePluginStateRange InDestination, FGameFeatureStateTransitionComplete OnFeatureStateTransitionComplete, FDelegateHandle* OutCallbackHandle /*= nullptr*/)
{
	check(IsValidDestinationState(InDestination.MinState));
	check(IsValidDestinationState(InDestination.MaxState));

	if (!InDestination.IsValid())
	{
		// Invalid range
		return false;
	}

	if (CurrentStateInfo.State == EGameFeaturePluginState::Terminal && !InDestination.Contains(EGameFeaturePluginState::Terminal))
	{
		// Can't tranistion away from terminal state
		return false;
	}

	if (!IsRunning())
	{
		// Not running so any new range is acceptable

		if (OutCallbackHandle)
		{
			OutCallbackHandle->Reset();
		}

		FDestinationGameFeaturePluginState* CurrState = AllStates[CurrentStateInfo.State]->AsDestinationState();

		if (InDestination.Contains(CurrentStateInfo.State))
		{
			OnFeatureStateTransitionComplete.ExecuteIfBound(this, MakeValue());
			return true;
		}
		
		if (CurrentStateInfo.State < InDestination)
		{
			FDestinationGameFeaturePluginState* MinDestState = AllStates[InDestination.MinState]->AsDestinationState();
			FDelegateHandle CallbackHandle = MinDestState->OnDestinationStateReached.Add(MoveTemp(OnFeatureStateTransitionComplete));
			if (OutCallbackHandle)
			{
				*OutCallbackHandle = CallbackHandle;
			}
		}
		else if (CurrentStateInfo.State > InDestination)
		{
			FDestinationGameFeaturePluginState* MaxDestState = AllStates[InDestination.MaxState]->AsDestinationState();
			FDelegateHandle CallbackHandle = MaxDestState->OnDestinationStateReached.Add(MoveTemp(OnFeatureStateTransitionComplete));
			if (OutCallbackHandle)
			{
				*OutCallbackHandle = CallbackHandle;
			}
		}

		StateProperties.Destination = InDestination;
		UpdateStateMachine();

		return true;
	}

	if (TOptional<FGameFeaturePluginStateRange> NewDestination = StateProperties.Destination.Intersect(InDestination))
	{
		// The machine is already running so we can only transition to this range if it overlaps with our current range.
		// We can satisfy both ranges in this case.

		if (OutCallbackHandle)
		{
			OutCallbackHandle->Reset();
		}

		if (CurrentStateInfo.State < StateProperties.Destination)
		{
			StateProperties.Destination = *NewDestination;

			if (InDestination.Contains(CurrentStateInfo.State))
			{
				OnFeatureStateTransitionComplete.ExecuteIfBound(this, MakeValue());
				return true;
			}

			FDestinationGameFeaturePluginState* MinDestState = AllStates[InDestination.MinState]->AsDestinationState();
			FDelegateHandle CallbackHandle = MinDestState->OnDestinationStateReached.Add(MoveTemp(OnFeatureStateTransitionComplete));
			if (OutCallbackHandle)
			{
				*OutCallbackHandle = CallbackHandle;
			}
		}
		else if(CurrentStateInfo.State > StateProperties.Destination)
		{
			StateProperties.Destination = *NewDestination;

			if (InDestination.Contains(CurrentStateInfo.State))
			{
				OnFeatureStateTransitionComplete.ExecuteIfBound(this, MakeValue());
				return true;
			}

			FDestinationGameFeaturePluginState* MaxDestState = AllStates[InDestination.MaxState]->AsDestinationState();
			FDelegateHandle CallbackHandle = MaxDestState->OnDestinationStateReached.Add(MoveTemp(OnFeatureStateTransitionComplete));
			if (OutCallbackHandle)
			{
				*OutCallbackHandle = CallbackHandle;
			}
		}
		else
		{
			checkf(false, TEXT("IsRunning() returned true but state machine has reached destination!"));
		}

		return true;
	}

	// The requested state range is completely outside the the current state range so reject the request
	return false;
}

bool UGameFeaturePluginStateMachine::TryCancel(FGameFeatureStateTransitionCanceled OnFeatureStateTransitionCanceled, FDelegateHandle* OutCallbackHandle /*= nullptr*/)
{
	if (!IsRunning())
	{
		return false;
	}

	StateProperties.bTryCancel = true;
	FDelegateHandle CallbackHandle = StateProperties.OnTransitionCanceled.Add(MoveTemp(OnFeatureStateTransitionCanceled));
	if(OutCallbackHandle)
	{
		*OutCallbackHandle = CallbackHandle;
	}

	const EGameFeaturePluginState CurrentState = GetCurrentState();
	AllStates[CurrentState]->TryCancelState();

	return true;
}

bool UGameFeaturePluginStateMachine::TryUpdatePluginURLData(const FString& InPluginURL)
{
	//Check if our current PluginURL information matches this new URL
	//and early out if no update is needed
	if (StateProperties.PluginIdentifier.ExactMatchesURL(InPluginURL))
	{
		return false;
	}

	const EGameFeaturePluginState CurrentState = GetCurrentState();
	return AllStates[CurrentState]->TryUpdateURLData(InPluginURL);
}

void UGameFeaturePluginStateMachine::RemovePendingTransitionCallback(FDelegateHandle InHandle)
{
	for (std::underlying_type<EGameFeaturePluginState>::type iState = 0;
		iState < EGameFeaturePluginState::MAX;
		++iState)
	{
		if (FDestinationGameFeaturePluginState* DestState = AllStates[iState]->AsDestinationState())
		{
			if (DestState->OnDestinationStateReached.Remove(InHandle))
			{
				break;
			}
		}
	}
}

void UGameFeaturePluginStateMachine::RemovePendingTransitionCallback(void* DelegateObject)
{
	for (std::underlying_type<EGameFeaturePluginState>::type iState = 0;
		iState < EGameFeaturePluginState::MAX;
		++iState)
	{
		if (FDestinationGameFeaturePluginState* DestState = AllStates[iState]->AsDestinationState())
		{
			if (DestState->OnDestinationStateReached.RemoveAll(DelegateObject))
			{
				break;
			}
		}
	}
}

void UGameFeaturePluginStateMachine::RemovePendingCancelCallback(FDelegateHandle InHandle)
{
	StateProperties.OnTransitionCanceled.Remove(InHandle);
}

void UGameFeaturePluginStateMachine::RemovePendingCancelCallback(void* DelegateObject)
{
	StateProperties.OnTransitionCanceled.RemoveAll(DelegateObject);
}

const FString& UGameFeaturePluginStateMachine::GetGameFeatureName() const
{
	FString PluginFilename;
	if (!StateProperties.PluginName.IsEmpty())
	{
		return StateProperties.PluginName;
	}
	else
	{
		return StateProperties.PluginIdentifier.GetFullPluginURL();
	}
}

const FString& UGameFeaturePluginStateMachine::GetPluginURL() const
{
	return StateProperties.PluginIdentifier.GetFullPluginURL();
}

const FString& UGameFeaturePluginStateMachine::GetPluginName() const
{
	return StateProperties.PluginName;
}

bool UGameFeaturePluginStateMachine::GetPluginFilename(FString& OutPluginFilename) const
{
	OutPluginFilename = StateProperties.PluginInstalledFilename;
	return !OutPluginFilename.IsEmpty();
}

EGameFeaturePluginState UGameFeaturePluginStateMachine::GetCurrentState() const
{
	return GetCurrentStateInfo().State;
}

FGameFeaturePluginStateRange UGameFeaturePluginStateMachine::GetDestination() const
{
	return StateProperties.Destination;
}

const FGameFeaturePluginStateInfo& UGameFeaturePluginStateMachine::GetCurrentStateInfo() const
{
	return CurrentStateInfo;
}

bool UGameFeaturePluginStateMachine::IsRunning() const
{
	return !StateProperties.Destination.Contains(CurrentStateInfo.State);
}

bool UGameFeaturePluginStateMachine::IsStatusKnown() const
{
	return GetCurrentState() == EGameFeaturePluginState::ErrorUnavailable || 
		   GetCurrentState() == EGameFeaturePluginState::Uninstalling || 
		   GetCurrentState() == EGameFeaturePluginState::ErrorUninstalling ||
		   GetCurrentState() >= EGameFeaturePluginState::StatusKnown;
}

bool UGameFeaturePluginStateMachine::IsAvailable() const
{
	ensure(IsStatusKnown());
	return GetCurrentState() >= EGameFeaturePluginState::StatusKnown;
}

UGameFeatureData* UGameFeaturePluginStateMachine::GetGameFeatureDataForActivePlugin()
{
	if (GetCurrentState() == EGameFeaturePluginState::Active)
	{
		return StateProperties.GameFeatureData;
	}

	return nullptr;
}

UGameFeatureData* UGameFeaturePluginStateMachine::GetGameFeatureDataForRegisteredPlugin(bool bCheckForRegistering /*= false*/)
{
	const EGameFeaturePluginState CurrentState = GetCurrentState();

	if (CurrentState >= EGameFeaturePluginState::Registered || (bCheckForRegistering && (CurrentState == EGameFeaturePluginState::Registering)))
	{
		return StateProperties.GameFeatureData;
	}

	return nullptr;
}

bool UGameFeaturePluginStateMachine::IsValidTransitionState(EGameFeaturePluginState InState) const
{
	check(InState != EGameFeaturePluginState::MAX);
	return AllStates[InState]->GetStateType() == EGameFeaturePluginStateType::Transition;
}

bool UGameFeaturePluginStateMachine::IsValidDestinationState(EGameFeaturePluginState InDestinationState) const
{
	check(InDestinationState != EGameFeaturePluginState::MAX);
	return AllStates[InDestinationState]->GetStateType() == EGameFeaturePluginStateType::Destination;
}

bool UGameFeaturePluginStateMachine::IsValidErrorState(EGameFeaturePluginState InDestinationState) const
{
	check(InDestinationState != EGameFeaturePluginState::MAX);
	return AllStates[InDestinationState]->GetStateType() == EGameFeaturePluginStateType::Error;
}

void UGameFeaturePluginStateMachine::UpdateStateMachine()
{
	EGameFeaturePluginState CurrentState = GetCurrentState();
	if (bInUpdateStateMachine)
	{
		UE_LOG(LogGameFeatures, Verbose, TEXT("Game feature state machine skipping update for %s in ::UpdateStateMachine. Current State: %s"), *GetGameFeatureName(), *UE::GameFeatures::ToString(CurrentState));
		return;
	}

	TOptional<TGuardValue<bool>> ScopeGuard(InPlace, bInUpdateStateMachine, true);
	
	using StateIt = std::underlying_type<EGameFeaturePluginState>::type;

	auto DoCallbacks = [this](const UE::GameFeatures::FResult& Result, StateIt Begin, StateIt End)
	{
		for (StateIt iState = Begin; iState < End; ++iState)
		{
			if (FDestinationGameFeaturePluginState* DestState = AllStates[iState]->AsDestinationState())
			{
				// Use a local callback on the stack. If SetDestination() is called from the callback then we don't want to stomp the callback
				// for the new state transition request.
				// Callback from terminal state could also trigger a GC that would destroy the state machine
				FDestinationGameFeaturePluginState::FOnDestinationStateReached LocalOnDestinationStateReached(MoveTemp(DestState->OnDestinationStateReached));
				DestState->OnDestinationStateReached.Clear();

				LocalOnDestinationStateReached.Broadcast(this, Result);
			}
		}
	};

	auto DoCallback = [&DoCallbacks](const UE::GameFeatures::FResult& Result, StateIt InState)
	{
		DoCallbacks(Result, InState, InState + 1);
	};

	bool bKeepProcessing = false;
	int32 NumTransitions = 0;
	const int32 MaxTransitions = 10000;
	do
	{
		bKeepProcessing = false;

		FGameFeaturePluginStateStatus StateStatus;
		AllStates[CurrentState]->UpdateState(StateStatus);

		if (StateStatus.TransitionToState == CurrentState)
		{
			UE_LOG(LogGameFeatures, Fatal, TEXT("Game feature state %s transitioning to itself. GameFeature: %s"), *UE::GameFeatures::ToString(CurrentState), *GetGameFeatureName());
		}

		if (StateStatus.TransitionToState != EGameFeaturePluginState::Uninitialized)
		{
			UE_LOG(LogGameFeatures, Verbose, TEXT("Game feature '%s' transitioning state (%s -> %s)"), *GetGameFeatureName(), *UE::GameFeatures::ToString(CurrentState), *UE::GameFeatures::ToString(StateStatus.TransitionToState));
			AllStates[CurrentState]->EndState();
			CurrentStateInfo = FGameFeaturePluginStateInfo(StateStatus.TransitionToState);
			CurrentState = StateStatus.TransitionToState;
			check(CurrentState != EGameFeaturePluginState::MAX);
			AllStates[CurrentState]->BeginState();

			if (CurrentState == EGameFeaturePluginState::Terminal)
			{
				// Remove from gamefeature subsystem before calling back in case this GFP is reloaded on callback,
				// but make sure we don't get destroyed from a GC during a callback
				UGameFeaturesSubsystem::Get().BeginTermination(this);
			}

			if (StateProperties.bTryCancel && AllStates[CurrentState]->GetStateType() != EGameFeaturePluginStateType::Transition)
			{
				StateProperties.Destination = FGameFeaturePluginStateRange(CurrentState);

				StateProperties.bTryCancel = false;
				bKeepProcessing = false;

				// Make sure bInUpdateStateMachine is not set while processing callbacks if we are at our destination
				ScopeGuard.Reset();

				// For all callbacks, return the CanceledResult
				DoCallbacks(UE::GameFeatures::CanceledResult, 0, EGameFeaturePluginState::MAX);

				// Must be called after transtition callbacks, UGameFeaturesSubsystem::ChangeGameFeatureTargetStateComplete may remove the this machine from the subsystem
				FGameFeaturePluginStateMachineProperties::FOnTransitionCanceled LocalOnTransitionCanceled(MoveTemp(StateProperties.OnTransitionCanceled));
				StateProperties.OnTransitionCanceled.Clear();
				LocalOnTransitionCanceled.Broadcast(this);
			}
			else if (const bool bError = !StateStatus.TransitionResult.HasValue(); bError)
			{
				check(IsValidErrorState(CurrentState));
				StateProperties.Destination = FGameFeaturePluginStateRange(CurrentState);

				bKeepProcessing = false;
				
				// Make sure bInUpdateStateMachine is not set while processing callbacks if we are at our destination
				ScopeGuard.Reset();

				// In case of an error, callback all possible callbacks
				DoCallbacks(StateStatus.TransitionResult, 0, EGameFeaturePluginState::MAX);
			}
			else
			{
				bKeepProcessing = AllStates[CurrentState]->GetStateType() == EGameFeaturePluginStateType::Transition || !StateProperties.Destination.Contains(CurrentState);
				if (!bKeepProcessing)
				{
					// Make sure bInUpdateStateMachine is not set while processing callbacks if we are at our destination
					ScopeGuard.Reset();
				}

				DoCallback(StateStatus.TransitionResult, CurrentState);
			}

			if (CurrentState == EGameFeaturePluginState::Terminal)
			{
				check(bKeepProcessing == false);
				// Now that callbacks are done this machine can be cleaned up
				UGameFeaturesSubsystem::Get().FinishTermination(this);
				MarkAsGarbage();
			}
		}

		if (NumTransitions++ > MaxTransitions)
		{
			UE_LOG(LogGameFeatures, Fatal, TEXT("Infinite loop in game feature state machine transitions. Current state %s. GameFeature: %s"), *UE::GameFeatures::ToString(CurrentState), *GetGameFeatureName());
		}
	} while (bKeepProcessing);
}

void UGameFeaturePluginStateMachine::UpdateCurrentStateProgress(float Progress)
{
	CurrentStateInfo.Progress = Progress;
}

FGameFeaturePluginStateMachineProperties::FGameFeaturePluginStateMachineProperties(
	const FString& InPluginURL,
	const FGameFeaturePluginStateRange& DesiredDestination,
	const FGameFeaturePluginRequestUpdateStateMachine& RequestUpdateStateMachineDelegate,
	const FGameFeatureStateProgressUpdate& FeatureStateProgressUpdateDelegate)
	: PluginIdentifier(InPluginURL)
	, Destination(DesiredDestination)
	, OnRequestUpdateStateMachine(RequestUpdateStateMachineDelegate)
	, OnFeatureStateProgressUpdate(FeatureStateProgressUpdateDelegate)
{
}

EGameFeaturePluginProtocol FGameFeaturePluginStateMachineProperties::GetPluginProtocol() const
{
	if (PluginIdentifier.PluginProtocol != EGameFeaturePluginProtocol::Unknown)
	{
		return PluginIdentifier.PluginProtocol;
	}
	else
	{
		ensureAlwaysMsgf(false, TEXT("No cached valid EGameFeaturePluginProtocol in our PluginIdentifier!"));
		return UGameFeaturesSubsystem::GetPluginURLProtocol(PluginIdentifier.GetFullPluginURL());
	}
}

FInstallBundlePluginProtocolMetaData::FInstallBundlePluginProtocolMetaData()
{
	ResetToDefaults();
}

void FInstallBundlePluginProtocolMetaData::ResetToDefaults()
{
	InstallBundles.Reset();
	VersionNum = FDefaultValues::CurrentVersionNum;
	bUninstallBeforeTerminate = FDefaultValues::Default_bUninstallBeforeTerminate;
	bUserPauseDownload = FDefaultValues::Default_bUserPauseDownload;
	InstallBundleFlags = FDefaultValues::Default_InstallBundleFlags;
}

FString FInstallBundlePluginProtocolMetaData::ToString() const
{
	FString ReturnedString;

	//Always encode InstallBundles
	ReturnedString = LexToString(EGameFeatureInstallBundleProtocolOptions::Bundles) + UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator;
	for (const FName& BundleName : InstallBundles)
	{
		ReturnedString.Append(BundleName.ToString());
	}

	//Always encode version
	ReturnedString.Append(FString::Printf(TEXT("%s%s%s%d"), UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, *LexToString(EGameFeatureInstallBundleProtocolOptions::Version), UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator, VersionNum));

	//Encode InstallBundleFlags if not the default
	if (InstallBundleFlags != FDefaultValues::Default_InstallBundleFlags)
	{
		ReturnedString.Append(FString::Printf(TEXT("%s%s%s%d"), UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, *LexToString(EGameFeatureInstallBundleProtocolOptions::Flags), UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator, static_cast<uint32>(InstallBundleFlags)));
	}

	if (ReleaseInstallBundleFlags != FDefaultValues::Default_ReleaseInstallBundleFlags)
	{
		ReturnedString.Append(FString::Printf(TEXT("%s%s%s%d"), UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, *LexToString(EGameFeatureInstallBundleProtocolOptions::ReleaseFlags), UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator, static_cast<uint32>(ReleaseInstallBundleFlags)));
	}

	//Encode bUninstallBeforeTerminate if not the default
	if (bUninstallBeforeTerminate != FDefaultValues::Default_bUninstallBeforeTerminate)
	{
		ReturnedString.Append(FString::Printf(TEXT("%s%s%s%s"), UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, *LexToString(EGameFeatureInstallBundleProtocolOptions::UninstallBeforeTerminate), UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator, *LexToString(bUninstallBeforeTerminate)));
	}

	//Encode bUserPauseDownload if not the default
	if (bUserPauseDownload != FDefaultValues::Default_bUserPauseDownload)
	{
		ReturnedString.Append(FString::Printf(TEXT("%s%s%s%s"), UE::GameFeatures::PluginURLStructureInfo::OptionSeperator, *LexToString(EGameFeatureInstallBundleProtocolOptions::UserPauseDownload), UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator, *LexToString(bUserPauseDownload)));
	}

	static_assert(static_cast<uint8>(EGameFeatureInstallBundleProtocolOptions::Count) == 6, "Update this function to handle the newly added EGameFeatureInstallBundleProtocolOptions value!");

	return ReturnedString;
}

bool FInstallBundlePluginProtocolMetaData::FromString(const FString& URLString, FInstallBundlePluginProtocolMetaData& Metadata)
{
	bool bParseSuccess = true;
	TArray<FString> URLOptions;
	URLString.ParseIntoArray(URLOptions, UE::GameFeatures::PluginURLStructureInfo::OptionSeperator);

	Metadata.ResetToDefaults();

	if (URLOptions.Num() > 0)
	{
		ensureAlwaysMsgf(URLOptions[0].StartsWith(UE::GameFeatures::GameFeaturePluginProtocolPrefix(EGameFeaturePluginProtocol::InstallBundle)), TEXT("Unexpected URL Format! Expected Protocol and uplugin information at the beginning of the URL %s"), *URLString);

		//Parse through our URLOptions. Start at ParsingIndex 1 as option 0 should always be the .uplugin path that doesn't contain metadata
		for (int ParsingIndex = 1; ParsingIndex < URLOptions.Num(); ++ParsingIndex)
		{
			const FString& URLOption = URLOptions[ParsingIndex];
	
			TArray<FString> OptionStrings;
			URLOption.ParseIntoArray(OptionStrings, UE::GameFeatures::PluginURLStructureInfo::OptionAssignOperator);

			if (OptionStrings.Num() > 0)
			{
				EGameFeatureInstallBundleProtocolOptions OptionEnum = LexFromString(OptionStrings[0]);

				switch (OptionEnum)
				{
					case EGameFeatureInstallBundleProtocolOptions::Bundles:
					{
						if (OptionStrings.Num() != 2)
						{
							bParseSuccess = false;
							UE_LOG(LogGameFeatures, Error, TEXT("Error parsing InstallBundle protocol options URL %s . No Valid Bundle List Found!"), *URLString);
						}
						else
						{
							TArray<FString> BundleNames;
							OptionStrings[1].ParseIntoArray(BundleNames, TEXT(","));

							Metadata.InstallBundles.Reserve(BundleNames.Num());
							for (FString& BundleNameString : BundleNames)
		{
								Metadata.InstallBundles.Add(*BundleNameString);
							}
						}

						break;
					}

					case EGameFeatureInstallBundleProtocolOptions::Version:
					{
						if (OptionStrings.Num() != 2)
						{
							bParseSuccess = false;
							UE_LOG(LogGameFeatures, Warning, TEXT("Error parsing InstallBundle protocol options URL %s . Invalid Version Option!"), *URLString);
						}
						else
						{
							Metadata.VersionNum = FCString::Strtoi(*OptionStrings[1], nullptr, 10);
							if (Metadata.VersionNum != FDefaultValues::CurrentVersionNum)
							{
								ensureAlwaysMsgf(false, TEXT("Mismatched version number for URL %s ! URL was generated on a previous version and might be missing data!"));
							}
						}

						break;
					}

					case EGameFeatureInstallBundleProtocolOptions::Flags:
					{
						if (OptionStrings.Num() != 2)
						{
							bParseSuccess = false;
							UE_LOG(LogGameFeatures, Warning, TEXT("Error parsing InstallBundle protocol options URL %s . Invalid Flags Option!"), *URLString);
						}
						//Expect to parse uint32 flag version of this as a 2nd parameter
						else
						{
							Metadata.InstallBundleFlags = static_cast<EInstallBundleRequestFlags>(FCString::Strtoi(*OptionStrings[1], nullptr, 10));
						}

						break;
					}

					case EGameFeatureInstallBundleProtocolOptions::ReleaseFlags:
					{
						if (OptionStrings.Num() != 2)
						{
							bParseSuccess = false;
							UE_LOG(LogGameFeatures, Warning, TEXT("Error parsing InstallBundle protocol options URL %s . Invalid ReleaseFlags Option!"), *URLString);
						}
						//Expect to parse uint32 flag version of this as a 2nd parameter
						else
						{
							Metadata.ReleaseInstallBundleFlags= static_cast<EInstallBundleReleaseRequestFlags>(FCString::Strtoi(*OptionStrings[1], nullptr, 10));
						}

						break;
					}

					case EGameFeatureInstallBundleProtocolOptions::UninstallBeforeTerminate:
					{
						if (OptionStrings.Num() != 2)
						{
							bParseSuccess = false;
							UE_LOG(LogGameFeatures, Warning, TEXT("Error parsing InstallBundle protocol options URL %s . Invalid UninstallBeforeTerminate Option!"), *URLString);
						}
						else
						{
							Metadata.bUninstallBeforeTerminate = OptionStrings[1].ToBool();
						}

						break;
					}

					case EGameFeatureInstallBundleProtocolOptions::UserPauseDownload:
					{
						if (OptionStrings.Num() != 2)
						{
							bParseSuccess = false;
							UE_LOG(LogGameFeatures, Warning, TEXT("Error parsing InstallBundle protocol options URL %s . Invalid UserPauseDownload Option!"), *URLString);
						}
						else
						{
							Metadata.bUserPauseDownload = OptionStrings[1].ToBool();
						}

						break;
					}

					case EGameFeatureInstallBundleProtocolOptions::Count:
					default:
					{
						bParseSuccess = false;
						UE_LOG(LogGameFeatures, Error, TEXT("Error parsing InstallBundle protocol options for URL %s . Unknown Option %s"), *URLString, *URLOption);

						break;
					}
				}
			}
		}
	}
	//We require to have InstallBundle names for this URL parse to be correct
	if (Metadata.InstallBundles.Num() == 0)
	{
		bParseSuccess = false;
		UE_LOG(LogGameFeatures, Error, TEXT("Error parsing InstallBundle protocol options URL %s . No Bundle List Found!"), *URLString);
	}

	static_assert(static_cast<uint8>(EGameFeatureInstallBundleProtocolOptions::Count) == 6, "Update this function to handle the newly added EGameFeatureInstallBundleProtocolOptions value!");

	return bParseSuccess;
}

bool FGameFeaturePluginStateMachineProperties::ParseURL()
{
	if (!ensureMsgf(!PluginIdentifier.IdentifyingURLSubset.IsEmpty(), TEXT("Unexpected empty IdentifyingURLSubset while parsing URL!")))
	{
		return false;
	}

	//The current IdentifyingURLSubset IS the same as the PluginInstalledFilename, so just use the already parsed version
	PluginInstalledFilename = PluginIdentifier.IdentifyingURLSubset;
	PluginName = FPaths::GetBaseFilename(PluginInstalledFilename);

	if (PluginInstalledFilename.IsEmpty() || !PluginInstalledFilename.EndsWith(TEXT(".uplugin")))
	{
		ensureMsgf(false, TEXT("PluginInstalledFilename must have a uplugin extension. PluginInstalledFilename: %s"), *PluginInstalledFilename);
		return false;
	}

	//Do additional parsing of our Metadata from the options on our remaining URL
	if (GetPluginProtocol() == EGameFeaturePluginProtocol::InstallBundle)
	{
		FInstallBundlePluginProtocolMetaData& MetaData = *ProtocolMetadata.SetSubtype<FInstallBundlePluginProtocolMetaData>();
		if (!FInstallBundlePluginProtocolMetaData::FromString(PluginIdentifier.GetFullPluginURL(), MetaData))
		{
			ensureMsgf(false, TEXT("Failure to parse URL %s into a valid FInstallBundlePluginProtocolMetaData"), *PluginIdentifier.GetFullPluginURL());
			return false;
		}
	}

	static_assert(static_cast<uint8>(EGameFeaturePluginProtocol::Count) == 3, "Update FGameFeaturePluginStateMachineProperties::ParseURL to handle any new Metadata parsing required for new EGameFeaturePluginProtocol. If no metadata is required just increment this counter.");

	return true;
}

bool FGameFeaturePluginStateMachineProperties::ValidateURLUpdate(const FGameFeaturePluginStateMachineProperties& OldProperties) const
{
	bool bIsValidUpdate = false;

	//Should never change our Identifier or PluginProtocol
	bIsValidUpdate = ((PluginIdentifier == OldProperties.PluginIdentifier) &&
						(GetPluginProtocol() == OldProperties.GetPluginProtocol()));

	if (bIsValidUpdate && (GetPluginProtocol() == EGameFeaturePluginProtocol::InstallBundle))
	{
		if (!ensureAlwaysMsgf((ProtocolMetadata.HasSubtype<FInstallBundlePluginProtocolMetaData>() && 
								OldProperties.ProtocolMetadata.HasSubtype<FInstallBundlePluginProtocolMetaData>())
								,TEXT("Error with InstallBundle protocol FGameFeaturePluginStateMachineProperties having an invalid ProtocolMetadata. URL: %s")
								,*PluginIdentifier.GetFullPluginURL()))
		{
			
			return false;
		}

		const FInstallBundlePluginProtocolMetaData& NewMetaData = ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>();
		const FInstallBundlePluginProtocolMetaData& OldMetaData = OldProperties.ProtocolMetadata.GetSubtype<FInstallBundlePluginProtocolMetaData>();

		if (!ensureMsgf((NewMetaData.InstallBundles == OldMetaData.InstallBundles), TEXT("Unexpected change in InstallBundles when updating URL: %s to %s"), *PluginIdentifier.GetFullPluginURL(), *OldProperties.PluginIdentifier.GetFullPluginURL()))
		{
			bIsValidUpdate = false;
		}
	}

	return bIsValidUpdate;
}

