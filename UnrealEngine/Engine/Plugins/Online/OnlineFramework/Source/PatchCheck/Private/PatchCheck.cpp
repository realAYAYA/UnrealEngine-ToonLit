// Copyright Epic Games, Inc. All Rights Reserved.

#include "PatchCheck.h"
#include "PatchCheckModule.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Online.h"

DEFINE_LOG_CATEGORY(LogPatchCheck);

FPatchCheck& FPatchCheck::Get()
{
	static IPatchCheckModule* ConfiguredModule = nullptr;

	if (ConfiguredModule && ConfiguredModule->GetPatchCheck())
	{
		return *ConfiguredModule->GetPatchCheck();
	}

	FString ModuleName;
	if (GConfig->GetString(TEXT("PatchCheck"), TEXT("ModuleName"), ModuleName, GEngineIni))
	{
		if (FModuleManager::Get().ModuleExists(*ModuleName))
		{
			ConfiguredModule = FModuleManager::LoadModulePtr<IPatchCheckModule>(*ModuleName);
		}

		if (ConfiguredModule && ConfiguredModule->GetPatchCheck())
		{
			return *ConfiguredModule->GetPatchCheck();
		}
		else
		{
			// Couldn't find the configured module, fallback to the default
			ensureMsgf(false, TEXT("FPatchCheck: Couldn't find module with Name %s, using default"), ModuleName.IsEmpty() ? TEXT("None") : *ModuleName);
		}
	}

	// No override module configured, use default
	ConfiguredModule = &FModuleManager::LoadModuleChecked<IPatchCheckModule>(TEXT("PatchCheck"));
	return *ConfiguredModule->MakePatchCheck();
}

FPatchCheck::~FPatchCheck()
{

}

FPatchCheck::FPatchCheck()
{
	RefreshConfig();
}

void FPatchCheck::RefreshConfig()
{
	if (!GConfig->GetBool(TEXT("PatchCheck"), TEXT("bCheckPlatformOSSForUpdate"), bCheckPlatformOSSForUpdate, GEngineIni))
	{
		/** For backwards compatibility with UUpdateManager */
		if (GConfig->GetBool(TEXT("/Script/Hotfix.UpdateManager"), TEXT("bCheckPlatformOSSForUpdate"), bCheckPlatformOSSForUpdate, GEngineIni))
		{
			ensureMsgf(false, TEXT("UpdateManager::bCheckPlatformOSSForUpdate is deprecated, Set FPatchCheck::bCheckPlatformOSSForUpdate using section [PatchCheck] instead."));
		}
	}

	if (!GConfig->GetBool(TEXT("PatchCheck"), TEXT("bCheckOSSForUpdate"), bCheckOSSForUpdate, GEngineIni))
	{
		/** For backwards compatibility with UUpdateManager */
		if (GConfig->GetBool(TEXT("/Script/Hotfix.UpdateManager"), TEXT("bCheckOSSForUpdate"), bCheckOSSForUpdate, GEngineIni))
		{
			ensureMsgf(false, TEXT("UpdateManager::bCheckOSSForUpdate is deprecated, Set FPatchCheck::bCheckOSSForUpdate using section [PatchCheck] instead."));
		}
	}
}

void FPatchCheck::StartPatchCheck()
{
	if (bIsCheckInProgress)
		return;

	RefreshConfig();

	if (bCheckPlatformOSSForUpdate && IOnlineSubsystem::GetByPlatform() != nullptr)
	{
		bIsCheckInProgress = true;
		StartPlatformOSSPatchCheck();
	}
	else if (bCheckOSSForUpdate)
	{
		bIsCheckInProgress = true;
		StartOSSPatchCheck();
	}
	else
	{
		UE_LOG(LogPatchCheck, Warning, TEXT("Patch check disabled for both Platform and Default OSS"));
	}
}

void FPatchCheck::AddEnvironmentWantsPatchCheckBackCompatDelegate(FName Tag, FEnvironmentWantsPatchCheck Delegate)
{
	BackCompatEnvironmentWantsPatchCheckDelegates.Emplace(Tag, MoveTemp(Delegate));
}

void FPatchCheck::RemoveEnvironmentWantsPatchCheckBackCompatDelegate(FName Tag)
{
	BackCompatEnvironmentWantsPatchCheckDelegates.Remove(Tag);
}

void FPatchCheck::StartPlatformOSSPatchCheck()
{
	EPatchCheckResult PatchResult = EPatchCheckResult::PatchCheckFailure;
	bool bStarted = false;

	IOnlineSubsystem* PlatformOnlineSub = IOnlineSubsystem::GetByPlatform();
	check(PlatformOnlineSub);
	IOnlineIdentityPtr PlatformOnlineIdentity = PlatformOnlineSub->GetIdentityInterface();
	if (PlatformOnlineIdentity.IsValid())
	{
		FUniqueNetIdPtr UserId = GetFirstSignedInUser(PlatformOnlineIdentity);
#if PLATFORM_SWITCH
		// checking the CanPlayOnline privilege on switch will log the user in if required in all but the NotLoggedIn state
		const bool bCanCheckPlayOnlinePrivilege = UserId.IsValid() && (PlatformOnlineIdentity->GetLoginStatus(*UserId) != ELoginStatus::NotLoggedIn);
#else
		const bool bCanCheckPlayOnlinePrivilege = UserId.IsValid() && (PlatformOnlineIdentity->GetLoginStatus(*UserId) == ELoginStatus::LoggedIn);
#endif
		if (bCanCheckPlayOnlinePrivilege)
		{
			bStarted = true;
			PlatformOnlineIdentity->GetUserPrivilege(*UserId,
				EUserPrivileges::CanPlayOnline, IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate::CreateRaw(this, &FPatchCheck::OnCheckForPatchComplete, true));
		}
		else
		{
			UE_LOG(LogPatchCheck, Warning, TEXT("No valid platform user id when starting patch check!"));
			PatchResult = EPatchCheckResult::NoLoggedInUser;
		}
	}

	if (!bStarted)
	{
		// Any failure to call GetUserPrivilege will result in completing the flow via this path
		PatchCheckComplete(PatchResult);
	}
}

void FPatchCheck::StartOSSPatchCheck()
{
	EPatchCheckResult PatchResult = EPatchCheckResult::PatchCheckFailure;
	bool bStarted = false;

	// Online::GetIdentityInterface() can take a UWorld for correctness, but that only matters in PIE right now
	// and update checks should never happen in PIE currently.
	IOnlineIdentityPtr IdentityInt = Online::GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
		// User could be invalid for "before title/login" check, underlying code doesn't need a valid user currently
		FUniqueNetIdPtr UserId = IdentityInt->CreateUniquePlayerId(TEXT("InvalidUser"));
		if (UserId.IsValid())
		{
			bStarted = true;
			IdentityInt->GetUserPrivilege(*UserId,
				EUserPrivileges::CanPlayOnline, IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate::CreateRaw(this, &FPatchCheck::OnCheckForPatchComplete, false));
		}
	}

	if (!bStarted)
	{
		// Any failure to call GetUserPrivilege will result in completing the flow via this path
		PatchCheckComplete(PatchResult);
	}
}

bool FPatchCheck::EnvironmentWantsPatchCheck() const
{
	if (BackCompatEnvironmentWantsPatchCheckDelegates.Num() > 0)
	{
		for (const TPair<FName, FEnvironmentWantsPatchCheck>& Pair : BackCompatEnvironmentWantsPatchCheckDelegates)
		{
			if (Pair.Value.IsBound() && Pair.Value.Execute())
			{
				return true;
			}
		}
	}

	return false;
}

bool FPatchCheck::SkipPatchCheck() const
{
	// Does the environment care about patch checks (LIVE, STAGE, etc)
	bool bEnvironmentWantsPatchCheck = EnvironmentWantsPatchCheck();

	// Can always opt in to a check
	const bool bForcePatchCheck = FParse::Param(FCommandLine::Get(), TEXT("ForcePatchCheck"));

	// Prevent a patch check on editor builds 
	const bool bSkipDueToEditor = UE_EDITOR;

	// Prevent a patch check on dedicated server. UpdateManager also doesn't do a patch check on dedicated server.
	const bool bSkipDueToDedicatedServer = IsRunningDedicatedServer();

	// prevent a check when running unattended
	const bool bSkipDueToUnattended = FApp::IsUnattended();

	// Explicitly skipping the check
	const bool bForceSkipCheck = FParse::Param(FCommandLine::Get(), TEXT("SkipPatchCheck"));
	const bool bSkipPatchCheck = !bForcePatchCheck && (!bEnvironmentWantsPatchCheck || bSkipDueToEditor || bSkipDueToDedicatedServer || bForceSkipCheck || bSkipDueToUnattended);

	return bSkipPatchCheck;
}

void FPatchCheck::OnCheckForPatchComplete(const FUniqueNetId& UniqueId, EUserPrivileges::Type Privilege, uint32 PrivilegeResult, bool bConsoleCheck)
{
	UE_LOG(LogPatchCheck, Verbose, TEXT("[OnCheckForPatchComplete] Privilege=%d PrivilegeResult=%d"), (uint32)Privilege, PrivilegeResult);

	EPatchCheckResult Result = EPatchCheckResult::NoPatchRequired;
	if (Privilege == EUserPrivileges::CanPlayOnline)
	{
		if (bConsoleCheck || !SkipPatchCheck())
		{
			if (PrivilegeResult & (uint32)IOnlineIdentity::EPrivilegeResults::RequiredSystemUpdate)
			{
				Result = EPatchCheckResult::PatchRequired;
			}
			else if (PrivilegeResult & (uint32)IOnlineIdentity::EPrivilegeResults::RequiredPatchAvailable)
			{
				Result = EPatchCheckResult::PatchRequired;
			}
			else if (PrivilegeResult & ((uint32)IOnlineIdentity::EPrivilegeResults::UserNotLoggedIn | (uint32)IOnlineIdentity::EPrivilegeResults::UserNotFound))
			{
				Result = EPatchCheckResult::NoLoggedInUser;
			}
			else if (PrivilegeResult & (uint32)IOnlineIdentity::EPrivilegeResults::GenericFailure)
			{
				CA_CONSTANT_IF(PATCH_CHECK_FAIL_ON_GENERIC_FAILURE)
				{
					Result = EPatchCheckResult::PatchCheckFailure;
				}
				else
				{
					// Skip console backend failures
					Result = EPatchCheckResult::NoPatchRequired;
				}
			}
		}
	}

	if (bCheckOSSForUpdate && bConsoleCheck && Result == EPatchCheckResult::NoPatchRequired)
	{
		// We perform both checks in this case
		StartOSSPatchCheck();
		return;
	}

	PatchCheckComplete(Result);
}

void FPatchCheck::PatchCheckComplete(EPatchCheckResult PatchResult)
{
	OnComplete.Broadcast(PatchResult);
	bIsCheckInProgress = false;
}
