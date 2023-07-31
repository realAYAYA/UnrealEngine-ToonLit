// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityInterfaceIOS.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemIOS.h"
#include "OnlineError.h"
#include "IOS/IOSAppDelegate.h"
#import "OnlineAppStoreUtils.h"

FOnlineIdentityIOS::FOnlineIdentityIOS()
	: UniqueNetId(nullptr)
{
}

FOnlineIdentityIOS::FOnlineIdentityIOS(FOnlineSubsystemIOS* InSubsystem)
	: UniqueNetId(nullptr)
	, Subsystem(InSubsystem)
{
}

FUniqueNetIdIOSPtr FOnlineIdentityIOS::GetLocalPlayerUniqueId() const
{
	return UniqueNetId;
}

void FOnlineIdentityIOS::SetLocalPlayerUniqueId(const FUniqueNetIdIOSPtr& UniqueId)
{
	UniqueNetId = UniqueId;
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityIOS::GetUserAccount(const FUniqueNetId& UserId) const
{
	// not implemented
	TSharedPtr<FUserOnlineAccount> Result;
	return Result;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityIOS::GetAllUserAccounts() const
{
	// not implemented
	TArray<TSharedPtr<FUserOnlineAccount> > Result;
	return Result;
}

// Make sure IOnlineIdentity::Login is not called directly on iOS. Please use IOnlineExternalUI::ShowLoginUI instead.
bool FOnlineIdentityIOS::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	bool bStartedLogin = false;

	// Since the iOS login code may show a UI, ShowLoginUI is a better fit here. Also, note that the ConnectToService blueprint
	// node that calls Login is deprecated (there's a new ShowExternalLoginUI node meant to replace it).

	GKLocalPlayer* GKLocalUser = [GKLocalPlayer localPlayer];

	// Was the login handled by Game Center
	if( GKLocalUser && 
		GKLocalUser.isAuthenticated )
	{
		// Now logged in
		bStartedLogin = true;
		if ([GKPlayer respondsToSelector:@selector(scopedIDsArePersistent)] == YES)
		{
			if ([GKLocalUser scopedIDsArePersistent])
			{
				const FString PlayerId(FString(FOnlineSubsystemIOS::GetPlayerId(GKLocalUser)));

				UniqueNetId = FUniqueNetIdIOS::Create(PlayerId);
				TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UniqueNetId, TEXT(""));

				UE_LOG_ONLINE_IDENTITY(Log, TEXT("The user %s has logged into Game Center"), *PlayerId);
			}
			else
			{
				// ID is not persistent across multiple game sessions, consider as not logged in
				FString ErrorMessage = TEXT("The user could not be authenticated with a persistent id by Game Center");
				UE_LOG_ONLINE_IDENTITY(Log, TEXT("%s"), *ErrorMessage);

				TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdIOS::EmptyId(), *ErrorMessage);
			}
		}
		else
		{
			const FString PlayerId(FString(FOnlineSubsystemIOS::GetPlayerId(GKLocalUser)));

			UniqueNetId = FUniqueNetIdIOS::Create( PlayerId );
			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UniqueNetId, TEXT(""));

			UE_LOG_ONLINE_IDENTITY(Log, TEXT("The user %s has logged into Game Center"), *PlayerId);
		}
	}
	else
	{
		// Trigger the login event on the main thread.
		bStartedLogin = true;
		dispatch_async(dispatch_get_main_queue(), ^
		{
			[[GKLocalPlayer localPlayer] setAuthenticateHandler:(^(UIViewController* viewcontroller, NSError *error)
			{
				// The login process has completed.
				if (viewcontroller == nil)
				{
					bool bWasSuccessful = false;
					FString ErrorMessage;
					TOptional<FString> PlayerId;

					if (error)
					{
						// We did not complete authentication without error, we are not logged in - Game Center is not available
						NSString *errstr = [error localizedDescription];
						UE_LOG_ONLINE_IDENTITY(Warning, TEXT("Game Center login has failed. %s]"), *FString(errstr));

						ErrorMessage = TEXT("An error occured authenticating the user by Game Center");
						UE_LOG_ONLINE_IDENTITY(Log, TEXT("%s"), *ErrorMessage);
					}
					else if ([GKLocalPlayer localPlayer].isAuthenticated == YES)
					{
						GKLocalPlayer* GKLocalUserAuth = [GKLocalPlayer localPlayer];

						/* Perform additional tasks for the authenticated player here */
						if ([GKPlayer respondsToSelector:@selector(scopedIDsArePersistent)] == YES)
						{
							if ([GKLocalUserAuth scopedIDsArePersistent])
							{
								PlayerId = FString(FOnlineSubsystemIOS::GetPlayerId(GKLocalUserAuth));

								bWasSuccessful = true;
								UE_LOG_ONLINE_IDENTITY(Log, TEXT("The user %s has logged into Game Center"), *PlayerId.GetValue());
							}
							else
							{
								// ID is not persistent across multiple game sessions, consider as not logged in
								ErrorMessage = TEXT("The user could not be authenticated with a persistent id by Game Center");
								UE_LOG_ONLINE_IDENTITY(Log, TEXT("%s"), *ErrorMessage);
							}
						}
						else
						{
							PlayerId = FString(FOnlineSubsystemIOS::GetPlayerId(GKLocalUserAuth));

							bWasSuccessful = true;
							UE_LOG_ONLINE_IDENTITY(Log, TEXT("The user %s has logged into Game Center"), *PlayerId.GetValue());
						}
					}
					else
					{
						ErrorMessage = TEXT("The user could not be authenticated by Game Center");
						UE_LOG_ONLINE_IDENTITY(Log, TEXT("%s"), *ErrorMessage);
					}

					// Report back to the game thread whether this succeeded.
					[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
					{
						if (PlayerId.IsSet())
						{
							UniqueNetId = FUniqueNetIdIOS::Create(PlayerId.GetValue());
						}
						else
						{
							UniqueNetId.Reset();
						}

						FUniqueNetIdIOSRef UniqueIdForUser = UniqueNetId.IsValid() ? UniqueNetId.ToSharedRef() : FUniqueNetIdIOS::EmptyId();
						TriggerOnLoginCompleteDelegates(LocalUserNum, bWasSuccessful, *UniqueIdForUser, *ErrorMessage);

						return true;
					}];
				}
				else
				{
					// Game Center has provided a view controller for us to login, we present it.
					[[IOSAppDelegate GetDelegate].IOSController
						presentViewController:viewcontroller animated:YES completion:nil];
				}
			})];
		});
	}
	
	return bStartedLogin;
}

bool FOnlineIdentityIOS::Logout(int32 LocalUserNum)
{
	UniqueNetId.Reset();
	TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	return true;
}

bool FOnlineIdentityIOS::AutoLogin(int32 LocalUserNum)
{
	return Login( LocalUserNum, FOnlineAccountCredentials() );
}

ELoginStatus::Type FOnlineIdentityIOS::GetLoginStatus(int32 LocalUserNum) const
{
	ELoginStatus::Type LoginStatus = ELoginStatus::NotLoggedIn;

	if(LocalUserNum < MAX_LOCAL_PLAYERS && GetLocalGameCenterUser() != NULL && GetLocalGameCenterUser().isAuthenticated == YES)
	{
		LoginStatus = ELoginStatus::LoggedIn;
	}

	return LoginStatus;
}

ELoginStatus::Type FOnlineIdentityIOS::GetLoginStatus(const FUniqueNetId& UserId) const 
{
	ELoginStatus::Type LoginStatus = ELoginStatus::NotLoggedIn;

	if(GetLocalGameCenterUser() != NULL && GetLocalGameCenterUser().isAuthenticated == YES)
	{
		LoginStatus = ELoginStatus::LoggedIn;
	}

	return LoginStatus;
}

FUniqueNetIdPtr FOnlineIdentityIOS::GetUniquePlayerId(int32 LocalUserNum) const
{
	return UniqueNetId;
}

FUniqueNetIdPtr FOnlineIdentityIOS::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if( Bytes && Size == sizeof(uint64) )
	{
		int32 StrLen = FCString::Strlen((TCHAR*)Bytes);
		if (StrLen > 0)
		{
			FString StrId((TCHAR*)Bytes);
			return FUniqueNetIdIOS::Create(StrId);
		}
	}
	
	return NULL;
}

FUniqueNetIdPtr FOnlineIdentityIOS::CreateUniquePlayerId(const FString& Str)
{
	return FUniqueNetIdIOS::Create(Str);
}

FString FOnlineIdentityIOS::GetPlayerNickname(int32 LocalUserNum) const
{
	if (LocalUserNum < MAX_LOCAL_PLAYERS && GetLocalGameCenterUser() != NULL)
	{
		NSString* PersonaName = [GetLocalGameCenterUser() alias];
		
		if (PersonaName != nil)
		{
			return FString(PersonaName);
		}
	}

	return FString();
}

FString FOnlineIdentityIOS::GetPlayerNickname(const FUniqueNetId& UserId) const 
{
	if (GetLocalGameCenterUser() != NULL)
	{
		NSString* PersonaName = [GetLocalGameCenterUser() alias];
		
		if (PersonaName != nil)
		{
			return FString(PersonaName);
		}
	}

	return FString();
}

FString FOnlineIdentityIOS::GetAuthToken(int32 LocalUserNum) const
{
	FString ResultToken;
	return ResultToken;
}

void FOnlineIdentityIOS::RevokeAuthToken(const FUniqueNetId& UserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	UE_LOG_ONLINE_IDENTITY(Display, TEXT("FOnlineIdentityIOS::RevokeAuthToken not implemented"));
	FUniqueNetIdRef UserIdRef(UserId.AsShared());
	Subsystem->ExecuteNextTick([UserIdRef, Delegate]()
	{
		Delegate.ExecuteIfBound(*UserIdRef, FOnlineError(FString(TEXT("RevokeAuthToken not implemented"))));
	});
}

void FOnlineIdentityIOS::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	if (Privilege == EUserPrivileges::CanPlayOnline)
	{
		FUniqueNetIdRef SharedUserId = UserId.AsShared();
		FOnQueryAppBundleIdResponse completionDelegate = FOnQueryAppBundleIdResponse::CreateLambda([this, SharedUserId, Privilege, Delegate](NSDictionary* ResponseDict)
		{
			UE_LOG_ONLINE_IDENTITY(Log, TEXT("GetUserPrivilege Complete"));
																									   
			uint32 Result = (uint32)EPrivilegeResults::GenericFailure;
			if (ResponseDict != nil && [ResponseDict[@"resultCount"] integerValue] == 1)
			{
				// Get local bundle information
				NSDictionary* infoDictionary = [[NSBundle mainBundle] infoDictionary];
				FString localAppId = FString(infoDictionary[@"CFBundleIdentifier"]);
				FString localVersionString = FString(infoDictionary[@"CFBundleShortVersionString"]);
				UE_LOG_ONLINE_IDENTITY(Log, TEXT("Local: %s %s"), *localAppId, *localVersionString);

				// Get remote bundle information
				FString remoteAppId = FString([[[ResponseDict objectForKey:@"results"] objectAtIndex:0] objectForKey:@"bundleId"]);
				FString remoteVersionString = FString([[[ResponseDict objectForKey:@"results"] objectAtIndex:0] objectForKey:@"version"]);
				UE_LOG_ONLINE_IDENTITY(Log, TEXT("Remote: %s %s"), *remoteAppId, *remoteVersionString);

				if (localAppId == remoteAppId)
				{
					TArray<FString> LocalVersionParts;
					localVersionString.ParseIntoArray(LocalVersionParts, TEXT("."));

					TArray<FString> RemoteVersionParts;
					remoteVersionString.ParseIntoArray(RemoteVersionParts, TEXT("."));

					if (LocalVersionParts.Num() >= 2 &&
						RemoteVersionParts.Num() >= 2)
					{
						Result = (uint32)EPrivilegeResults::NoFailures;

						if (LocalVersionParts[0] != RemoteVersionParts[0] ||
							LocalVersionParts[1] != RemoteVersionParts[1])
						{
							UE_LOG_ONLINE_IDENTITY(Log, TEXT("Needs Update"));
							Result = (uint32)EPrivilegeResults::RequiredPatchAvailable;
						}
						else
						{
							const FString LocalHotfixVersion = LocalVersionParts.IsValidIndex(2) ? LocalVersionParts[2] : TEXT("0");
							const FString RemoteHotfixVersion = RemoteVersionParts.IsValidIndex(2) ? RemoteVersionParts[2] : TEXT("0");

							if (LocalHotfixVersion != RemoteHotfixVersion)
							{
								UE_LOG_ONLINE_IDENTITY(Log, TEXT("Needs Update"));
								Result = (uint32)EPrivilegeResults::RequiredPatchAvailable;
							}
							else
							{
								UE_LOG_ONLINE_IDENTITY(Log, TEXT("Does NOT Need Update"));
							}
						}
					}
				}
				else
				{
					UE_LOG_ONLINE_IDENTITY(Log, TEXT("BundleId does not match local bundleId"));
				}
			}
			else
			{
				UE_LOG_ONLINE_IDENTITY(Log, TEXT("GetUserPrivilege invalid response"));
			}

			Subsystem->ExecuteNextTick([Delegate, SharedUserId, Privilege, Result]()
			{
				Delegate.ExecuteIfBound(*SharedUserId, Privilege, Result);
			});
		});
		
		FAppStoreUtils* AppStoreUtils = Subsystem->GetAppStoreUtils();
		if (AppStoreUtils)
		{
			[AppStoreUtils queryAppBundleId: completionDelegate];
		}
	 }
	 else
	 {
		  Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
	 }
}

FPlatformUserId FOnlineIdentityIOS::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& InUniqueNetId) const
{
	for (int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
	{
		auto CurrentUniqueId = GetUniquePlayerId(i);
		if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == InUniqueNetId))
		{
			return GetPlatformUserIdFromLocalUserNum(i);
		}
	}

	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityIOS::GetAuthType() const
{
	return TEXT("");
}
