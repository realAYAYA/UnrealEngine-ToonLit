// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemIOS.h"
#include "IOS/IOSAppDelegate.h"
#import "OnlineStoreKitHelper.h"
#import "OnlineAppStoreUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Stats/Stats.h"

FOnlineSubsystemIOS::FOnlineSubsystemIOS(FName InInstanceName)
	: FOnlineSubsystemImpl(IOS_SUBSYSTEM, InInstanceName)
	, StoreHelper(nil)
	, AppStoreHelper(nil)
{
	StoreHelper = nil;
}

IOnlineSessionPtr FOnlineSubsystemIOS::GetSessionInterface() const
{
	return SessionInterface;
}

IOnlineFriendsPtr FOnlineSubsystemIOS::GetFriendsInterface() const
{
	return FriendsInterface;
}

IOnlinePartyPtr FOnlineSubsystemIOS::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemIOS::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemIOS::GetSharedCloudInterface() const
{
	return SharedCloudInterface;
}

IOnlineUserCloudPtr FOnlineSubsystemIOS::GetUserCloudInterface() const
{
	return UserCloudInterface;
}

IOnlineLeaderboardsPtr FOnlineSubsystemIOS::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemIOS::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemIOS::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemIOS::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemIOS::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineTitleFilePtr FOnlineSubsystemIOS::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemIOS::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineStoreV2Ptr FOnlineSubsystemIOS::GetStoreV2Interface() const
{
	return StoreV2Interface;
}

IOnlinePurchasePtr FOnlineSubsystemIOS::GetPurchaseInterface() const
{
	return PurchaseInterface;
}

IOnlineEventsPtr FOnlineSubsystemIOS::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemIOS::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

IOnlineSharingPtr FOnlineSubsystemIOS::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemIOS::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemIOS::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemIOS::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemIOS::GetChatInterface() const
{
	return nullptr;
}

IOnlineStatsPtr FOnlineSubsystemIOS::GetStatsInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemIOS::GetTurnBasedInterface() const
{
	return TurnBasedInterface;
}

IOnlineTournamentPtr FOnlineSubsystemIOS::GetTournamentInterface() const
{
	return nullptr;
}

bool FOnlineSubsystemIOS::Init() 
{
	bool bSuccessfullyStartedUp = true;
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemIOS::Init()"));
	
	if( !IsEnabled() )
	{
		UE_LOG_ONLINE(Warning, TEXT("All iOS online features have been disabled in the system settings"));
		bSuccessfullyStartedUp = false;
	}
	else
	{
		SessionInterface = MakeShareable(new FOnlineSessionIOS(this));
		IdentityInterface = MakeShareable(new FOnlineIdentityIOS(this));

		if (IsGameCenterEnabled())
		{
			FriendsInterface = MakeShareable(new FOnlineFriendsIOS(this));
			LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsIOS(this));
			AchievementsInterface = MakeShareable(new FOnlineAchievementsIOS(this));
			ExternalUIInterface = MakeShareable(new FOnlineExternalUIIOS(this));
			TurnBasedInterface = MakeShareable(new FOnlineTurnBasedIOS());
		}

		UserCloudInterface = MakeShareable(new FOnlineUserCloudInterfaceIOS());
		SharedCloudInterface = MakeShareable(new FOnlineSharedCloudInterfaceIOS());

		if (IsInAppPurchasingEnabled())
		{
			StoreV2Interface = MakeShareable(new FOnlineStoreIOS(this));
			PurchaseInterface = MakeShareable(new FOnlinePurchaseIOS(this));
			InitStoreKitHelper();
		}

		if (UserCloudInterface && IsCloudKitEnabled())
		{
			FString IOSCloudKitSyncStrategy = "";
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("IOSCloudKitSyncStrategy"), IOSCloudKitSyncStrategy, GEngineIni);

			if (!IOSCloudKitSyncStrategy.Equals("None"))
			{
				UserCloudInterface->InitCloudSave(IOSCloudKitSyncStrategy.Equals("Always"));
			}
		}

		InitAppStoreHelper();
	}

	return bSuccessfullyStartedUp;
}

void FOnlineSubsystemIOS::InitStoreKitHelper()
{
	StoreHelper = [[FStoreKitHelperV2 alloc] init];
	[StoreHelper retain];
	
	// Give each interface a chance to bind to the store kit helper
	StoreV2Interface->InitStoreKit(StoreHelper);
	PurchaseInterface->InitStoreKit(StoreHelper);

}

void FOnlineSubsystemIOS::CleanupStoreKitHelper()
{
	[StoreHelper release];
	StoreHelper = nil;
}

void FOnlineSubsystemIOS::InitAppStoreHelper()
{
	AppStoreHelper = [[FAppStoreUtils alloc] init];
	[AppStoreHelper retain];
}

void FOnlineSubsystemIOS::CleanupAppStoreHelper()
{
	[AppStoreHelper release];
	AppStoreHelper = nil;
}

FAppStoreUtils* FOnlineSubsystemIOS::GetAppStoreUtils()
{
	return AppStoreHelper;
}

bool FOnlineSubsystemIOS::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FOnlineSubsystemIOS_Tick);

	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (SessionInterface.IsValid())
	{
		SessionInterface->Tick(DeltaTime);
	}
	return true;
}

FText FOnlineSubsystemIOS::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemIOS", "OnlineServiceName", "Game Center");
}

bool FOnlineSubsystemIOS::Shutdown() 
{
	bool bSuccessfullyShutdown = true;
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemIOS::Shutdown()"));

	bSuccessfullyShutdown = FOnlineSubsystemImpl::Shutdown();
	
#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		UE_LOG_ONLINE(Display, TEXT(#Interface));\
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}
	
	DESTRUCT_INTERFACE(SessionInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
	DESTRUCT_INTERFACE(FriendsInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(TurnBasedInterface);
	DESTRUCT_INTERFACE(UserCloudInterface);
	DESTRUCT_INTERFACE(SharedCloudInterface);
	DESTRUCT_INTERFACE(StoreV2Interface);
	DESTRUCT_INTERFACE(PurchaseInterface);

#undef DESTRUCT_INTERFACE
	
	// Cleanup after the interfaces are free
	CleanupStoreKitHelper();
	CleanupAppStoreHelper();
	
	return bSuccessfullyShutdown;
}

FString FOnlineSubsystemIOS::GetAppId() const 
{
	return TEXT( "" );
}

bool FOnlineSubsystemIOS::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)  
{
	bool bWasHandled = false;
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		bWasHandled = true;
	}
	else
	{ 
		if (FParse::Command(&Cmd, TEXT("PURCHASE")))
		{
			bWasHandled = HandlePurchaseExecCommands(InWorld, Cmd, Ar);
		}
	}

	return bWasHandled;
}

bool FOnlineSubsystemIOS::HandlePurchaseExecCommands(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;

	if (FParse::Command(&Cmd, TEXT("DUMPAPPRECEIPT")))
	{
		[StoreHelper dumpAppReceipt];
	}

	return bWasHandled;
}

bool FOnlineSubsystemIOS::IsEnabled() const
{
	bool bEnableGameCenter = IsGameCenterEnabled();

	const bool bEnableCloudKit = IsCloudKitEnabled();

	const bool bIsInAppPurchasingEnabled = IsInAppPurchasingEnabled();
	const bool bIsEnabledByConfig = FOnlineSubsystemImpl::IsEnabled(); // TODO: Do we want to enable this by this config?
	
	return bEnableGameCenter || bEnableCloudKit || bIsInAppPurchasingEnabled || bIsEnabledByConfig;
}

bool FOnlineSubsystemIOS::IsGameCenterEnabled()
{
	bool bEnableGameCenter = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableGameCenterSupport"), bEnableGameCenter, GEngineIni);

	return bEnableGameCenter;
}

bool FOnlineSubsystemIOS::IsCloudKitEnabled()
{
	bool bEnableCloudKit;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableCloudKitSupport"), bEnableCloudKit, GEngineIni);

	return bEnableCloudKit;
}

bool FOnlineSubsystemIOS::IsInAppPurchasingEnabled()
{
	bool bEnableIAP = false;
	GConfig->GetBool(TEXT("OnlineSubsystemIOS.Store"), TEXT("bSupportsInAppPurchasing"), bEnableIAP, GEngineIni);
	
	bool bEnableIAP1 = false;
	GConfig->GetBool(TEXT("OnlineSubsystemIOS.Store"), TEXT("bSupportInAppPurchasing"), bEnableIAP1, GEngineIni);
	return bEnableIAP || bEnableIAP1;
}

NSString* FOnlineSubsystemIOS::GetPlayerId(GKPlayer* Player)
{
	return Player.gamePlayerID;
}

NSString* FOnlineSubsystemIOS::GetPlayerId(GKLocalPlayer* Player)
{
	return Player.gamePlayerID;
}

