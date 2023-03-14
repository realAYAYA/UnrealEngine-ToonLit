// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServicesEOS.h"

#include "Online/AuthEOS.h"
#include "Online/ExternalUIEOS.h"
#include "Online/SocialEOS.h"
#include "Online/PresenceEOS.h"
#include "Online/UserInfoEOS.h"
#include "Online/CommerceEOS.h"

namespace UE::Online {

FOnlineServicesEOS::FOnlineServicesEOS(FName InInstanceName)
	: Super(InInstanceName)
{
}

void FOnlineServicesEOS::RegisterComponents()
{
	Components.Register<FAuthEOS>(*this);
	Components.Register<FExternalUIEOS>(*this);
	Components.Register<FSocialEOS>(*this);
	Components.Register<FPresenceEOS>(*this);
	Components.Register<FUserInfoEOS>(*this);
	Components.Register<FCommerceEOS>(*this);

	Super::RegisterComponents();
}

/* UE::Online */ }
