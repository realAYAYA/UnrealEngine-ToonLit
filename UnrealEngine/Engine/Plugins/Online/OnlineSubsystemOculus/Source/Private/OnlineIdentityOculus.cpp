// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityOculus.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "OnlineSubsystemOculusPackage.h"
#include "OnlineError.h"

bool FUserOnlineAccountOculus::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = AdditionalAuthData.Find(AttrName);
	if (FoundAttr != nullptr)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

bool FUserOnlineAccountOculus::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = UserAttributes.Find(AttrName);
	if (FoundAttr != nullptr)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

bool FUserOnlineAccountOculus::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	UserAttributes[AttrName] = AttrValue;
	return true;
}

bool FOnlineIdentityOculus::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	FString ErrorStr;
	TSharedPtr<FUserOnlineAccountOculus> UserAccountPtr;

	// valid local player index
	// MAX_LOCAL_PLAYERS == 1
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		ErrorStr = FString::Printf(TEXT("Invalid LocalUserNum=%d"), LocalUserNum);
	}
	else
	{
		FUniqueNetIdRef* UserId = UserIds.Find(LocalUserNum);
		if (UserId == nullptr)
		{
			auto OculusId = ovr_GetLoggedInUserID();
			if (OculusId == 0)
			{
				ErrorStr = TEXT("Not currently logged into Oculus.  Make sure Oculus is running and you are entitled to the app.");
			}
			else
			{
				// Immediately add the Oculus ID to our cache list
				UserIds.Add(LocalUserNum, FUniqueNetIdOculus::Create(OculusId));
				OculusSubsystem.AddRequestDelegate(
					ovr_User_GetLoggedInUser(),
					FOculusMessageOnCompleteDelegate::CreateRaw(this, &FOnlineIdentityOculus::OnLoginComplete, LocalUserNum));
				return true;
			}
		}
		else
		{
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, **UserId, *ErrorStr);
		}
	}

	if (!ErrorStr.IsEmpty())
	{
		UE_LOG_ONLINE_IDENTITY(Warning, TEXT("Failed Oculus login. %s"), *ErrorStr);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdOculus::EmptyId(), ErrorStr);
	}

	return false;
}

void FOnlineIdentityOculus::OnLoginComplete(ovrMessageHandle Message, bool bIsError, int32 LocalUserNum)
{
	FString ErrorStr;
	if (bIsError)
	{
		auto Error = ovr_Message_GetError(Message);
		auto ErrorMessage = ovr_Error_GetMessage(Error);
		ErrorStr = FString(ErrorMessage);		
	}
	else
	{
		auto User = ovr_Message_GetUser(Message);
		auto Id = ovr_User_GetID(User);
		FString Name(ovr_User_GetOculusID(User));

		FUniqueNetIdRef LocalUserId = FUniqueNetIdOculus::Create(Id);
		if (!LocalUserId->IsValid())
		{
			ErrorStr = FString(TEXT("Unable to get a valid ID"));
		}
		else
		{
			UserIds.Add(LocalUserNum, LocalUserId);

			// update/add cached entry for user
			UserAccounts.Add(LocalUserId, TSharedRef<FUserOnlineAccountOculus>(new FUserOnlineAccountOculus(LocalUserId, Name)));

			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *LocalUserId, *ErrorStr);
			TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *LocalUserId);
			return;
		}
	}

	TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdOculus::EmptyId(), *ErrorStr);
}

bool FOnlineIdentityOculus::Logout(int32 LocalUserNum)
{
	FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		// remove cached user account
		UserAccounts.Remove(UserId.ToSharedRef());
		// remove cached user id
		UserIds.Remove(LocalUserNum);
		// not async but should call completion delegate anyway
		TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
		TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserId);
		return true;
	}
	else
	{
		TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	}
	return false;
}

bool FOnlineIdentityOculus::AutoLogin(int32 LocalUserNum)
{
	FString LoginStr;
	FString PasswordStr;
	FString TypeStr;

	return Login(LocalUserNum, FOnlineAccountCredentials(TypeStr, LoginStr, PasswordStr));
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityOculus::GetUserAccount(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> Result;

	const TSharedRef<FUserOnlineAccountOculus>* FoundUserAccount = UserAccounts.Find(UserId.AsShared());
	if (FoundUserAccount != nullptr)
	{
		Result = *FoundUserAccount;
	}

	return Result;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityOculus::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;

	for (TUniqueNetIdMap<TSharedRef<FUserOnlineAccountOculus>>::TConstIterator It(UserAccounts); It; ++It)
	{
		Result.Add(It.Value());
	}

	return Result;
}

FUniqueNetIdPtr FOnlineIdentityOculus::GetUniquePlayerId(int32 LocalUserNum) const
{
	if (const FUniqueNetIdRef* FoundId = UserIds.Find(LocalUserNum))
	{
		return *FoundId;
	}
	return nullptr;
}

FUniqueNetIdPtr FOnlineIdentityOculus::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Bytes && Size == sizeof(ovrID))
	{
		uint64* RawUniqueId = (uint64*)Bytes;
		ovrID OculusId(*RawUniqueId);
		return FUniqueNetIdOculus::Create(OculusId);
	}
	return nullptr;
}

FUniqueNetIdPtr FOnlineIdentityOculus::CreateUniquePlayerId(const FString& Str)
{
	return FUniqueNetIdOculus::Create(Str);
}

ELoginStatus::Type FOnlineIdentityOculus::GetLoginStatus(int32 LocalUserNum) const
{
	FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetLoginStatus(*UserId);
	}
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityOculus::GetLoginStatus(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(UserId);
	if (UserAccount.IsValid() &&
		UserAccount->GetUserId()->IsValid())
	{
		return ELoginStatus::LoggedIn;
	}
	return ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentityOculus::GetPlayerNickname(int32 LocalUserNum) const
{
	FUniqueNetIdPtr UniqueId = GetUniquePlayerId(LocalUserNum);
	if (UniqueId.IsValid())
	{
		return GetPlayerNickname(*UniqueId);
	}

	return TEXT("OCULUS USER");
}

FString FOnlineIdentityOculus::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	auto UserAccount = GetUserAccount(UserId);
	if (UserAccount.IsValid())
	{
		return UserAccount->GetDisplayName();
	}
	return UserId.ToString();
}

FString FOnlineIdentityOculus::GetAuthToken(int32 LocalUserNum) const
{
	// TODO: implement
	return FString();
}

void FOnlineIdentityOculus::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	UE_LOG_ONLINE_IDENTITY(Display, TEXT("FOnlineIdentityOculus::RevokeAuthToken not implemented"));
	FUniqueNetIdRef UserIdRef(UserId.AsShared());
	OculusSubsystem.ExecuteNextTick([UserIdRef, Delegate]()
	{
		Delegate.ExecuteIfBound(*UserIdRef, FOnlineError(FString(TEXT("RevokeAuthToken not implemented"))));
	});
}

FOnlineIdentityOculus::FOnlineIdentityOculus(FOnlineSubsystemOculus& InSubsystem)
	: OculusSubsystem(InSubsystem)
{
	// Auto login the 0-th player
	FOnlineIdentityOculus::AutoLogin(0);
}

void FOnlineIdentityOculus::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	// Check for entitlement
	OculusSubsystem.AddRequestDelegate(
		ovr_Entitlement_GetIsViewerEntitled(),
		FOculusMessageOnCompleteDelegate::CreateLambda([UserId = UserId.AsShared(), Privilege, Delegate](ovrMessageHandle Message, bool bIsError)
		{
			uint32 PrivilegeResults = 0;

			// If the user failed the entitlement check, they have no privileges
			if (bIsError)
			{
				auto Error = ovr_Message_GetError(Message);
				FString ErrorMessage(ovr_Error_GetMessage(Error));
				UE_LOG_ONLINE_IDENTITY(Error, TEXT("Failed the entitlement check: %s"), *ErrorMessage);
				PrivilegeResults = static_cast<uint32>(IOnlineIdentity::EPrivilegeResults::UserNotFound);
			}
			else
			{
				UE_LOG_ONLINE_IDENTITY(Verbose, TEXT("User is entitled to app"));
				PrivilegeResults = static_cast<uint32>(IOnlineIdentity::EPrivilegeResults::NoFailures);
			}
			Delegate.ExecuteIfBound(*UserId, Privilege, PrivilegeResults);
		}));
}

FPlatformUserId FOnlineIdentityOculus::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	for (int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
	{
		auto CurrentUniqueId = GetUniquePlayerId(i);
		if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == UniqueNetId))
		{
			return GetPlatformUserIdFromLocalUserNum(i);
		}
	}

	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityOculus::GetAuthType() const
{
	return TEXT("Oculus");
}
