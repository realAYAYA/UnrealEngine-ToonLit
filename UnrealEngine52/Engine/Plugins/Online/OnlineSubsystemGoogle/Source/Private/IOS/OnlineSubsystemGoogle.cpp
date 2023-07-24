// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGoogle.h"
#include "OnlineSubsystemGooglePrivate.h"

#include "Misc/CoreDelegates.h"
#include "IOS/IOSAppDelegate.h"

#include "OnlineIdentityGoogle.h"
//#include "OnlineExternalUIInterfaceGoogle.h"
#import <GoogleSignIn/GoogleSignIn.h>

static void OnGoogleOpenURL(UIApplication* application, NSURL* url, NSString* sourceApplication, id annotation)
{
	bool bResult = [[GIDSignIn sharedInstance] handleURL:url
								       sourceApplication:sourceApplication
									   annotation:annotation];
	UE_LOG_ONLINE(Display, TEXT("OnGoogleOpenURL %s %d"), *FString(url.absoluteString), bResult);
}

static void OnGoogleAppDidBecomeActive()
{
	UE_LOG_ONLINE(Display, TEXT("OnGoogleAppDidBecomeActive"));

}

FOnlineSubsystemGoogle::FOnlineSubsystemGoogle(FName InInstanceName)
	: FOnlineSubsystemGoogleCommon(InInstanceName)
{
}

FOnlineSubsystemGoogle::~FOnlineSubsystemGoogle()
{
}

bool FOnlineSubsystemGoogle::Init()
{
	FIOSCoreDelegates::OnOpenURL.AddStatic(&OnGoogleOpenURL);
	FCoreDelegates::ApplicationHasReactivatedDelegate.AddStatic(&OnGoogleAppDidBecomeActive);

	if (FOnlineSubsystemGoogleCommon::Init())
	{
		FOnlineIdentityGooglePtr TempPtr = MakeShareable(new FOnlineIdentityGoogle(this));
		if (TempPtr->Init())
		{
			GoogleIdentity = TempPtr;
		}
		
		//GoogleExternalUI = MakeShareable(new FOnlineExternalUIGoogle(this));
		return GoogleIdentity.IsValid();
	}

	return false;
}

bool FOnlineSubsystemGoogle::Shutdown()
{
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemGoogle::Shutdown()"));
	return FOnlineSubsystemGoogleCommon::Shutdown();
}
