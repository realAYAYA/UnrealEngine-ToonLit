// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
 
// Module includes
#include "OnlineIdentityFacebookCommon.h"
#include "OnlineAccountFacebookCommon.h"
#include "OnlineSubsystemFacebookPackage.h"

#import "FacebookHelper.h"

class FOnlineSubsystemFacebook;

@class FBSDKAccessToken;
@class FBSDKProfile;
@class FFacebookHelper;

/** iOS implementation of a Facebook user account */
using FUserOnlineAccountFacebook  = FUserOnlineAccountFacebookCommon;

/**
 * Facebook service implementation of the online identity interface
 */
class FOnlineIdentityFacebook :
	public FOnlineIdentityFacebookCommon,
    public FIOSFacebookNotificationDelegate,
    public TSharedFromThis<FOnlineIdentityFacebook>
{

public:

	//~ Begin IOnlineIdentity Interface	
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	//~ End IOnlineIdentity Interface

public:

	/**
	 * Default constructor
	 */
	FOnlineIdentityFacebook(FOnlineSubsystemFacebook* InSubsystem);

	/**
	 * Destructor
	 */
	virtual ~FOnlineIdentityFacebook()
	{
	}

PACKAGE_SCOPE:

    /** Inits the ObjC bridge */
    void Init();

	/** Shutdown the interface */
	void Shutdown();

	/**
	 * Login user to Facebook, given a valid access token
	 *
	 * @param LocalUserNum local id of the requesting user
	 * @param AccessToken opaque Facebook supplied access token
	 */
	void Login(int32 LocalUserNum, const FString& AccessToken);

private:

	/**
	 * Delegate called when current permission request completes
	 *
	 * @param LocalUserNum user that made the request
	 * @param bWasSuccessful was the request successful
	 * @param NewPermissions array of all known permissions
	 */
	void OnRequestCurrentPermissionsComplete(int32 LocalUserNum, bool bWasSuccessful, const TArray<FSharingPermission>& NewPermissions);

	/**
	 * Generic callback for all attempts at login, called to end the attempt
	 *
	 * @param local id of the requesting user
	 * @param ErrorStr any error as a result of the login attempt
	 */
	void OnLoginAttemptComplete(int32 LocalUserNum, const FString& ErrorStr);

    virtual void OnFacebookTokenChange(FBSDKAccessToken* OldToken, FBSDKAccessToken* NewToken) override;
    virtual void OnFacebookUserIdChange() override;
    virtual void OnFacebookProfileChange(FBSDKProfile* OldProfile, FBSDKProfile* NewProfile) override;

    /** ObjC helper for access to SDK methods and callbacks */
	FFacebookHelper* FacebookHelper;

	/** The current state of our login */
	ELoginStatus::Type LoginStatus;

	/** Config based list of permission scopes to use when logging in */
	TArray<FString> ScopeFields;
};

typedef TSharedPtr<FOnlineIdentityFacebook, ESPMode::ThreadSafe> FOnlineIdentityFacebookPtr;
