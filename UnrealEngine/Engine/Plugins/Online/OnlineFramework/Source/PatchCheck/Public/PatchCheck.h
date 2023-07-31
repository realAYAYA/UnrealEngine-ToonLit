// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PatchCheckModule.h"
#include "Interfaces/OnlineIdentityInterface.h"

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
	virtual bool EnvironmentWantsPatchCheck() const;
	bool SkipPatchCheck() const;
	void OnCheckForPatchComplete(const FUniqueNetId& UniqueId, EUserPrivileges::Type Privilege, uint32 PrivilegeResult, bool bConsoleCheck);
	virtual void PatchCheckComplete(EPatchCheckResult PatchResult);

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

	friend class TPatchCheckModule<FPatchCheck>;
};
