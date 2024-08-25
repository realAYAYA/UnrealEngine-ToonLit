// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/OnlineIdentityInterface.h"

template<class PatchCheckClass> class TPatchCheckModule;

PATCHCHECK_API DECLARE_LOG_CATEGORY_EXTERN(LogPatchCheck, Log, All);

/**
 * Possible outcomes at the end of just the patch check
 */
enum class EPatchCheckResult : uint8
{
	/** No patch required */
	NoPatchRequired,
	/** Patch required to continue */
	PatchRequired,
	/** Logged in user required for a patch check */
	NoLoggedInUser,
	/** Patch check failed */
	PatchCheckFailure,
	Count,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPatchCheckComplete, EPatchCheckResult /*Result*/);

// For backwards compatibility only!
DECLARE_DELEGATE_RetVal(bool, FEnvironmentWantsPatchCheck);

class PATCHCHECK_API FPatchCheck
{
public:
	static FPatchCheck& Get();

	virtual ~FPatchCheck();

protected:
	FPatchCheck();

private:
	FPatchCheck(const FPatchCheck& Other) = delete;

	FPatchCheck& operator=(const FPatchCheck& Other) = delete;

public:
	void StartPatchCheck();

	void AddEnvironmentWantsPatchCheckBackCompatDelegate(FName Tag, FEnvironmentWantsPatchCheck Delegate);
	void RemoveEnvironmentWantsPatchCheckBackCompatDelegate(FName Tag);

	FOnPatchCheckComplete& GetOnComplete() { return OnComplete; }

protected:
	void RefreshConfig();
	virtual void StartPlatformOSSPatchCheck();
	virtual void StartOSSPatchCheck();
	void HandleOSSPatchCheck();
	virtual bool EnvironmentWantsPatchCheck() const;
	bool SkipPatchCheck() const;
	void OnCheckForPatchComplete(const FUniqueNetId& UniqueId, EUserPrivileges::Type Privilege, uint32 PrivilegeResult, bool bConsoleCheck);
	virtual void PatchCheckComplete(EPatchCheckResult PatchResult);

#if PATCH_CHECK_PLATFORM_ENVIRONMENT_DETECTION
	/**
	 * Platform specific implementation of platform environment detection
	 * @return true if the detection began.  false if the detection did not begin and we should continue the checks.
	 */
	bool DetectPlatformEnvironment();

	/**
	 * Platform specific callback for logging in on console which is needed for the platform environment detection
	 */
	virtual void DetectPlatformEnvironment_OnLoginConsoleComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
#endif
	/**
	 * Callback when detecting platform environment completes
	 */
	void OnDetectPlatformEnvironmentComplete(const FOnlineError& Result);

protected:

	FOnPatchCheckComplete OnComplete;

	/** For backwards compatibility with UUpdateManager */
	TMap<FName, FEnvironmentWantsPatchCheck> BackCompatEnvironmentWantsPatchCheckDelegates;

	/** Track whether we can start a new check */
	bool bIsCheckInProgress = false;

	/** Check the platform OSS for an update */	
	bool bCheckPlatformOSSForUpdate = true;
	/** Check the default OSS for an update */
	bool bCheckOSSForUpdate = true;

#if PATCH_CHECK_PLATFORM_ENVIRONMENT_DETECTION
	FDelegateHandle OnLoginConsoleCompleteHandle;
#endif

	/** true if we've already detected the backend environment */
	bool bPlatformEnvironmentDetected = !PATCH_CHECK_PLATFORM_ENVIRONMENT_DETECTION; // Default to true if we do not need to detect
	bool bPlatformEnvironmentDetectionEnabled = true;

	friend class TPatchCheckModule<FPatchCheck>;
};
