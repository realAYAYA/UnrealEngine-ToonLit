// Copyright Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineIdentityFacebook.h"
#include "OnlineSharingFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"
#include "Interfaces/OnlineSharingInterface.h"
#include "Interfaces/OnlineExternalUIInterface.h"

#include "Misc/ConfigCacheIni.h"
#include "IOS/IOSAsyncTask.h"

THIRD_PARTY_INCLUDES_START
#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKLoginKit/FBSDKLoginKit-Swift.h>
THIRD_PARTY_INCLUDES_END

///////////////////////////////////////////////////////////////////////////////////////
// FOnlineIdentityFacebook implementation

FOnlineIdentityFacebook::FOnlineIdentityFacebook(FOnlineSubsystemFacebook* InSubsystem)
	: FOnlineIdentityFacebookCommon(InSubsystem)
	, LoginStatus(ELoginStatus::NotLoggedIn)
{
	// Setup permission scope fields
	GConfig->GetArray(TEXT("OnlineSubsystemFacebook.OnlineIdentityFacebook"), TEXT("ScopeFields"), ScopeFields, GEngineIni);
	// always required fields
	ScopeFields.AddUnique(TEXT(PERM_PUBLIC_PROFILE));

}

void FOnlineIdentityFacebook::Init()
{
    FacebookHelper = [[FFacebookHelper alloc] initWithOwner: AsShared()];
}

void FOnlineIdentityFacebook::Shutdown()
{
    [FacebookHelper Shutdown];
	FacebookHelper = nil;
}

void FOnlineIdentityFacebook::OnFacebookTokenChange(FBSDKAccessToken* OldToken, FBSDKAccessToken* NewToken)
{
	UE_LOG_ONLINE_IDENTITY(Warning, TEXT("FOnlineIdentityFacebook::OnFacebookTokenChange Old: %p New: %p"), OldToken, NewToken);
}

void FOnlineIdentityFacebook::OnFacebookUserIdChange()
{
	UE_LOG_ONLINE_IDENTITY(Warning, TEXT("FOnlineIdentityFacebook::OnFacebookUserIdChange"));
}

void FOnlineIdentityFacebook::OnFacebookProfileChange(FBSDKProfile* OldProfile, FBSDKProfile* NewProfile)
{
	UE_LOG_ONLINE_IDENTITY(Warning, TEXT("FOnlineIdentityFacebook::OnFacebookProfileChange Old: %p New: %p"), OldProfile, NewProfile);
}

bool FOnlineIdentityFacebook::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	bool bTriggeredLogin = true;

	if (GetLoginStatus(LocalUserNum) != ELoginStatus::NotLoggedIn)
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *GetUniquePlayerId(LocalUserNum), TEXT("Already logged in"));
		return false;
	}

	ensure(LoginStatus == ELoginStatus::NotLoggedIn);

	dispatch_async(dispatch_get_main_queue(),^
		{
			FBSDKAccessToken *accessToken = [FBSDKAccessToken currentAccessToken];
			if (accessToken == nil)
			{
				FBSDKLoginManager* loginManager = [[FBSDKLoginManager alloc] init];
				// Start with iOS level account information, falls back to Native app, then web
				//loginManager.loginBehavior = FBSDKLoginBehaviorSystemAccount;
				NSMutableArray* Permissions = [[NSMutableArray alloc] initWithCapacity:ScopeFields.Num()];
				for (int32 ScopeIdx = 0; ScopeIdx < ScopeFields.Num(); ScopeIdx++)
				{
					NSString* ScopeStr = [NSString stringWithFString:ScopeFields[ScopeIdx]];
					[Permissions addObject: ScopeStr];
				}

				[loginManager logInWithPermissions:Permissions
					fromViewController:nil
					handler: ^(FBSDKLoginManagerLoginResult* result, NSError* error)
					{
						UE_LOG_ONLINE_IDENTITY(Display, TEXT("[FBSDKLoginManager logInWithReadPermissions]"));
						bool bSuccessfulLogin = false;

						FString ErrorStr;
						if(error)
						{
							ErrorStr = FString::Printf(TEXT("[%d] %s"), [error code], [error localizedDescription]);
							UE_LOG_ONLINE_IDENTITY(Display, TEXT("[FBSDKLoginManager logInWithReadPermissions = %s]"), *ErrorStr);

						}
						else if(result.isCancelled)
						{
							ErrorStr = LOGIN_CANCELLED;
							UE_LOG_ONLINE_IDENTITY(Display, TEXT("[FBSDKLoginManager logInWithReadPermissions = cancelled"));
						}						
						else
						{
							UE_LOG_ONLINE_IDENTITY(Display, TEXT("[FBSDKLoginManager logInWithReadPermissions = true]"));
							bSuccessfulLogin = true;
						}

                        TArray<FString> GrantedPermissions, DeclinedPermissions;

                        GrantedPermissions.Reserve(result.grantedPermissions.count);
                        for(NSString* permission in result.grantedPermissions)
                        {
                            GrantedPermissions.Add(permission);
                        }

                        DeclinedPermissions.Reserve(result.declinedPermissions.count);
                        for(NSString* permission in result.declinedPermissions)
                        {
                            DeclinedPermissions.Add(permission);
                        }

						const FString AccessToken([[result token] tokenString]);
						[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
						{
							// Trigger this on the game thread
							if (bSuccessfulLogin)
							{
                                TSharedPtr<FOnlineSharingFacebook> Sharing = StaticCastSharedPtr<FOnlineSharingFacebook>(FacebookSubsystem->GetSharingInterface());
                                Sharing->SetCurrentPermissions(GrantedPermissions, DeclinedPermissions);
								Login(LocalUserNum, AccessToken);
							}
							else
							{
								OnLoginAttemptComplete(LocalUserNum, ErrorStr);
							}

							return true;
						 }];
					}
				];
			}
			else
			{
				// Skip right to attempting to use the token to query user profile
				// Could fail with an expired auth token (eg. user revoked app)

				const FString AccessToken([accessToken tokenString]);
				[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
				{
					Login(LocalUserNum, AccessToken);
					return true;
				 }];
			}
		}
	);

	return bTriggeredLogin;	
}

void FOnlineIdentityFacebook::Login(int32 LocalUserNum, const FString& AccessToken)
{
	FOnProfileRequestComplete CompletionDelegate = FOnProfileRequestComplete::CreateLambda([this](int32 LocalUserNumFromRequest, bool bWasProfileRequestSuccessful, const FString& ErrorStr)
	{
		FOnRequestCurrentPermissionsComplete NextCompletionDelegate = FOnRequestCurrentPermissionsComplete::CreateLambda([this](int32 LocalUserNumFromPerms, bool bWerePermsSuccessful, const TArray<FSharingPermission>& Permissions)
		{
			OnRequestCurrentPermissionsComplete(LocalUserNumFromPerms, bWerePermsSuccessful, Permissions);
		});

		if (bWasProfileRequestSuccessful)
		{
			RequestCurrentPermissions(LocalUserNumFromRequest, NextCompletionDelegate);
		}
		else
		{
			OnLoginAttemptComplete(LocalUserNumFromRequest, ErrorStr);
		}
	});

	ProfileRequest(LocalUserNum, AccessToken, ProfileFields, CompletionDelegate);
}

void FOnlineIdentityFacebook::OnRequestCurrentPermissionsComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& NewPermissions)
{
	FString ErrorStr;
	if (!bWasSuccessful)
	{
		ErrorStr = TEXT("Failure to request current sharing permissions");
	}

	LoginStatus = bWasSuccessful ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn;
	OnLoginAttemptComplete(LocalUserNum, ErrorStr);
}

void FOnlineIdentityFacebook::OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr)
{
	if (LoginStatus == ELoginStatus::LoggedIn)
	{
		UE_LOG_ONLINE_IDENTITY(Display, TEXT("Facebook login was successful"));
		FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
		check(UserId.IsValid());
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserId, ErrorStr);
		TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserId);
	}
	else
	{
		const FString NewErrorStr(ErrorStr);
		// Clean up anything left behind from cached access tokens
		dispatch_async(dispatch_get_main_queue(),^
		{
			FBSDKLoginManager* loginManager = [[FBSDKLoginManager alloc] init];
			[loginManager logOut];

			[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
			 {
				// Trigger this on the game thread
				UE_LOG_ONLINE_IDENTITY(Display, TEXT("Facebook login failed: %s"), *NewErrorStr);

				FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
				if (UserId.IsValid())
				{
					// remove cached user account
					UserAccounts.Remove(UserId->ToString());
				}
				else
				{
					UserId = FUniqueNetIdFacebook::EmptyId();
				}
				// remove cached user id
				UserIds.Remove(LocalUserNum);

				TriggerOnLoginCompleteDelegates(LocalUserNum, false, *UserId, NewErrorStr);
				return true;
			 }];
		});
	}
}

bool FOnlineIdentityFacebook::Logout(int32 LocalUserNum)
{
	if ([FBSDKAccessToken currentAccessToken])
	{
		ensure(LoginStatus == ELoginStatus::LoggedIn);

		dispatch_async(dispatch_get_main_queue(),^
		{
			FBSDKLoginManager* loginManager = [[FBSDKLoginManager alloc] init];
			[loginManager logOut];

			[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
			{
				// Trigger this on the game thread
				FUniqueNetIdPtr UserId = GetUniquePlayerId(LocalUserNum);
				if (UserId.IsValid())
				{
					// remove cached user account
					UserAccounts.Remove(UserId->ToString());
				}
				else
				{
					UserId = FUniqueNetIdFacebook::EmptyId();
				}
				// remove cached user id
				UserIds.Remove(LocalUserNum);

				FacebookSubsystem->ExecuteNextTick([this, UserId, LocalUserNum]() {
					LoginStatus = ELoginStatus::NotLoggedIn;
					TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
					TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserId);
				});
				return true;
			 }];
		});
	}
	else
	{
		ensure(LoginStatus == ELoginStatus::NotLoggedIn);

		UE_LOG_ONLINE_IDENTITY(Warning, TEXT("No logged in user found for LocalUserNum=%d."), LocalUserNum);
		FacebookSubsystem->ExecuteNextTick([this, LocalUserNum](){
			TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		});
	}

	return true;
}


