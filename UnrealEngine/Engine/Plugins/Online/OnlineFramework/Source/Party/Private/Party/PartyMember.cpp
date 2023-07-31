// Copyright Epic Games, Inc. All Rights Reserved.

#include "Party/PartyMember.h"
#include "Party/SocialParty.h"
#include "User/SocialUser.h"
#include "SocialManager.h"
#include "SocialToolkit.h"

#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlinePartyInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PartyMember)

//////////////////////////////////////////////////////////////////////////
// PartyMemberRepData
//////////////////////////////////////////////////////////////////////////

void FPartyMemberRepData::SetOwningMember(const UPartyMember& InOwnerMember)
{
	OwnerMember = &InOwnerMember;
}

bool FPartyMemberRepData::CanEditData() const
{
	return OwnerMember.IsValid() && OwnerMember->IsLocalPlayer();
}

void FPartyMemberRepData::CompareAgainst(const FOnlinePartyRepDataBase& OldData) const
{
	const FPartyMemberRepData& TypedOldData = static_cast<const FPartyMemberRepData&>(OldData);

	ComparePlatformDataPlatform(TypedOldData);
	ComparePlatformDataUniqueId(TypedOldData);
	ComparePlatformDataSessionId(TypedOldData);
	CompareCrossplayPreference(TypedOldData);
	CompareJoinInProgressDataRequest(TypedOldData);
	CompareJoinInProgressDataResponses(TypedOldData);
}

const USocialParty* FPartyMemberRepData::GetOwnerParty() const
{
	return OwnerMember.IsValid() ? &OwnerMember->GetParty() : nullptr;
}

const UPartyMember* FPartyMemberRepData::GetOwningMember() const
{
	return OwnerMember.IsValid() ? OwnerMember.Get() : nullptr;
}

//////////////////////////////////////////////////////////////////////////
// PartyMember
//////////////////////////////////////////////////////////////////////////

UPartyMember::UPartyMember()
{
}

void UPartyMember::BeginDestroy()
{
	Super::BeginDestroy();

	if(!IsTemplate())
	{
		Shutdown();
	}
}

void UPartyMember::InitializePartyMember(const FOnlinePartyMemberConstRef& InOssMember, const FSimpleDelegate& OnInitComplete)
{
	checkf(MemberDataReplicator.IsValid(), TEXT("Child classes of UPartyMember MUST call MemberRepData.EstablishRepDataInstance with a valid FPartyMemberRepData struct instance in their constructor."));
	MemberDataReplicator->SetOwningMember(*this);

	if (ensure(!OssPartyMember.IsValid()))
	{
		OssPartyMember = InOssMember;
		OssPartyMember->OnMemberConnectionStatusChanged().AddUObject(this, &ThisClass::HandleMemberConnectionStatusChanged);
		OssPartyMember->OnMemberAttributeChanged().AddUObject(this, &ThisClass::HandleMemberAttributeChanged);
		USocialToolkit* OwnerToolkit = GetParty().GetSocialManager().GetSocialToolkit(OssPartyMember->GetUserId());
		// If we are not a local user then we simply get the first local user's toolkit
		if (OwnerToolkit == nullptr)
		{
			OwnerToolkit = GetParty().GetSocialManager().GetFirstLocalUserToolkit();
		}
		check(OwnerToolkit);

		OwnerToolkit->QueueUserDependentAction(InOssMember->GetUserId(),
			[this] (USocialUser& User)
			{
				SocialUser = &User;
			}, false);
		check(SocialUser);

		UE_LOG(LogParty, Log, TEXT("%s - QUDA returned SocialUser %s (%x)"), ANSI_TO_TCHAR(__FUNCTION__), *GetFullNameSafe(SocialUser), SocialUser);

		// Local player already has all the data they need, everyone else we want to wait for
		bHasReceivedInitialData = IsLocalPlayer();

		OnInitializationComplete().Add(OnInitComplete);
		UE_LOG(LogParty, Log, TEXT("%s - Registering Init Complete Handler for [%s], SocialUser: %s (%x)"), ANSI_TO_TCHAR(__FUNCTION__), *ToDebugString(), *GetFullNameSafe(SocialUser), SocialUser);
		SocialUser->RegisterInitCompleteHandler(FOnNewSocialUserInitialized::CreateUObject(this, &UPartyMember::HandleSocialUserInitialized));

		UE_LOG(LogParty, Verbose, TEXT("Created new party member [%s]"), *ToDebugString());
	}
}

void UPartyMember::InitializeLocalMemberRepData()
{
	UE_LOG(LogParty, Verbose, TEXT("Initializing rep data for local member [%s]"), *ToDebugString());

	MemberDataReplicator->SetPlatformDataPlatform(IOnlineSubsystem::GetLocalPlatformName());
	MemberDataReplicator->SetPlatformDataUniqueId(SocialUser->GetUserId(ESocialSubsystem::Platform));
	
	const USocialParty& CurrentParty = GetParty();
	
	FString JoinMethod;
	if (const USocialManager::FJoinPartyAttempt* JoinAttempt = CurrentParty.GetSocialManager().GetJoinAttemptInProgress(CurrentParty.GetPartyTypeId()))
	{
		JoinMethod = JoinAttempt->JoinMethod.ToString();
		UE_LOG(LogParty, Verbose, TEXT("Join method from join attempt for local member is %s."), *JoinMethod);
	}
	else
	{
		IOnlinePartyPtr PartyInterface = Online::GetPartyInterfaceChecked(GetWorld());
		FOnlinePartyDataConstPtr PartyMemberData = PartyInterface.IsValid() ? PartyInterface->GetPartyMemberData(*CurrentParty.GetOwningLocalUserId(), CurrentParty.GetPartyId(), *GetPrimaryNetId(), DefaultPartyDataNamespace) : nullptr;
		
		if (PartyMemberData.IsValid())
		{
			FVariantData AttrValue;
			if (PartyMemberData->GetAttribute(TEXT("JoinMethod"), AttrValue))
			{
				JoinMethod = AttrValue.ToString();
				UE_LOG(LogParty, Verbose, TEXT("Join method recovered from the Party member data is %s."), *JoinMethod);
			}
		}
	}

	MemberDataReplicator->SetJoinMethod(JoinMethod);
}

void UPartyMember::Shutdown()
{
	MemberDataReplicator.Reset();
}

bool UPartyMember::CanPromoteToLeader() const
{
	return GetParty().CanPromoteMember(*this);
}

bool UPartyMember::PromoteToPartyLeader()
{
	return GetParty().TryPromoteMember(*this);
}

bool UPartyMember::CanKickFromParty() const
{
	return GetParty().CanKickMember(*this);
}

bool UPartyMember::KickFromParty()
{
	return GetParty().TryKickMember(*this);
}

bool UPartyMember::IsInitialized() const
{
	return SocialUser->IsInitialized() && bHasReceivedInitialData;
}

USocialParty& UPartyMember::GetParty() const
{
	return *GetTypedOuter<USocialParty>();
}

FUniqueNetIdRepl UPartyMember::GetPrimaryNetId() const
{
	check(OssPartyMember.IsValid());
	return OssPartyMember->GetUserId();
}

USocialUser& UPartyMember::GetSocialUser() const
{
	check(SocialUser);
	return *SocialUser;
}

FString UPartyMember::GetDisplayName() const
{
	return OssPartyMember->GetDisplayName(GetRepData().GetPlatformDataPlatform());
}

FName UPartyMember::GetPlatformOssName() const
{
	return MemberDataReplicator->GetPlatformDataUniqueId().GetType();
}

FString UPartyMember::ToDebugString(bool bIncludePartyId) const
{
	FString MemberIdentifierStr;

#if UE_BUILD_SHIPPING
	MemberIdentifierStr = GetPrimaryNetId().ToDebugString();
#else
	// It's a whole lot easier to debug with real names when it's ok to do so
	MemberIdentifierStr = FString::Printf(TEXT("%s (%s)"), *GetDisplayName(), *GetPrimaryNetId().ToDebugString());
#endif
	
	if (bIncludePartyId)
	{
		return FString::Printf(TEXT("%s, Party (%s)"), *MemberIdentifierStr, *GetParty().GetPartyId().ToDebugString());
	}
	return MemberIdentifierStr;
}

bool UPartyMember::IsPartyLeader() const
{
	return GetParty().GetPartyLeader() == this;
}

bool UPartyMember::IsLocalPlayer() const
{
	return GetParty().GetSocialManager().IsLocalUser(GetPrimaryNetId(), ESocialSubsystem::Primary);
}

void UPartyMember::NotifyMemberDataReceived(const FOnlinePartyData& MemberData)
{
	UE_LOG(LogParty, VeryVerbose, TEXT("Received updated rep data for member [%s]"), *ToDebugString());

	check(MemberDataReplicator.IsValid());
	MemberDataReplicator.ProcessReceivedData(MemberData/*, bHasReceivedInitialData*/);

	if (!bHasReceivedInitialData)
	{
		bHasReceivedInitialData = true;
		if (SocialUser->IsInitialized())
		{
			FinishInitializing();
		}
	}
}

void UPartyMember::NotifyMemberPromoted()
{
	UE_LOG(LogParty, Verbose, TEXT("Member [%s] promoted to party leader."), *ToDebugString());
	OnMemberPromotedInternal();
}

void UPartyMember::NotifyMemberDemoted()
{
	UE_LOG(LogParty, Verbose, TEXT("Member [%s] is no longer party leader."), *ToDebugString());
	OnMemberDemotedInternal();
}

void UPartyMember::NotifyRemovedFromParty(EMemberExitedReason ExitReason)
{
	UE_LOG(LogParty, Verbose, TEXT("Member [%s] is no longer in the party. Reason = [%s]"), *ToDebugString(), ToString(ExitReason));
	OnRemovedFromPartyInternal(ExitReason);
}

void UPartyMember::FinishInitializing()
{
	//@todo DanH Party: The old UFortParty did this. Only used for Switch. Thing is, doesn't this need to be solved for all social users? Not just party members? #suggested
	SocialUser->SetUserLocalAttribute(ESocialSubsystem::Primary, USER_ATTR_PREFERRED_DISPLAYNAME, OssPartyMember->GetDisplayName());

	if (IsLocalPlayer())
	{
		InitializeLocalMemberRepData();
	}

	UE_LOG(LogParty, Verbose, TEXT("PartyMember [%s] is now fully initialized."), *ToDebugString());
	OnInitializationComplete().Broadcast();
	OnInitializationComplete().Clear();
}

void UPartyMember::OnMemberPromotedInternal()
{
	OnPromotedToLeader().Broadcast();
}

void UPartyMember::OnMemberDemotedInternal()
{
	OnDemoted().Broadcast();
}

void UPartyMember::OnRemovedFromPartyInternal(EMemberExitedReason ExitReason)
{
	OnLeftParty().Broadcast(ExitReason);
}

void UPartyMember::HandleSocialUserInitialized(USocialUser& InitializedUser)
{
	UE_LOG(LogParty, VeryVerbose, TEXT("PartyMember [%s]'s underlying SocialUser has been initialized"), *ToDebugString());
	if (bHasReceivedInitialData)
	{
		FinishInitializing();
	}
}

EMemberConnectionStatus UPartyMember::GetMemberConnectionStatus() const
{
	if (OssPartyMember.IsValid())
	{
		return OssPartyMember->MemberConnectionStatus;
	}
	return EMemberConnectionStatus::Uninitialized;
}

void UPartyMember::HandleMemberConnectionStatusChanged(const FUniqueNetId& ChangedUserId, const EMemberConnectionStatus NewMemberConnectionStatus, const EMemberConnectionStatus PreviousMemberConnectionStatus)
{
	OnMemberConnectionStatusChanged().Broadcast();
}

void UPartyMember::HandleMemberAttributeChanged(const FUniqueNetId& ChangedUserId, const FString& Attribute, const FString& NewValue, const FString& OldValue)
{
	if (Attribute == USER_ATTR_DISPLAYNAME)
	{
		OnDisplayNameChanged().Broadcast();
	}
}

