// Copyright Epic Games, Inc. All Rights Reserved.

#if USES_RESTFUL_FACEBOOK

#include "OnlineFriendsFacebookRest.h"
#include "OnlineSubsystemFacebookPrivate.h"

FOnlineFriendsFacebook::FOnlineFriendsFacebook(FOnlineSubsystemFacebook* InSubsystem) 
	: FOnlineFriendsFacebookCommon(InSubsystem)
{
}

FOnlineFriendsFacebook::~FOnlineFriendsFacebook()
{
}

#endif // USES_RESTFUL_FACEBOOK
