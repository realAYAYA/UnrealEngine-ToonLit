// Copyright Epic Games, Inc. All Rights Reserved.

#include "Party/PartyDataReplicator.h"
#include "Party/SocialParty.h"
#include "Party/PartyMember.h"
#include "OnlineSubsystemUtils.h"

void FPartyDataReplicatorHelper::ReplicateDataToMembers(const FOnlinePartyRepDataBase& RepDataInstance, const UScriptStruct& RepDataType, const FOnlinePartyData& ReplicationPayload)
{
	if (const USocialParty* OwnerParty = static_cast<const FOnlinePartyRepDataBase*>(&RepDataInstance)->GetOwnerParty())
	{
		FUniqueNetIdRepl LocalUserId = OwnerParty->GetOwningLocalUserId();
		IOnlinePartyPtr PartyInterface = Online::GetPartyInterface(OwnerParty->GetWorld());
		if (LocalUserId.IsValid() && PartyInterface.IsValid())
		{
			const FOnlinePartyId& PartyId = OwnerParty->GetPartyId();
			if (RepDataType.IsChildOf(FPartyRepData::StaticStruct()))
			{
				UE_LOG(LogParty, VeryVerbose, TEXT("Sending rep data update for party [%s]."), *OwnerParty->ToDebugString());
				PartyInterface->UpdatePartyData(*LocalUserId, OwnerParty->GetPartyId(), DefaultPartyDataNamespace, ReplicationPayload);
			}
			else if (RepDataType.IsChildOf(FPartyMemberRepData::StaticStruct()))
			{
				if (const UPartyMember* Owner = static_cast<const FOnlinePartyRepDataBase*>(&RepDataInstance)->GetOwningMember())
				{
					LocalUserId = Owner->GetPrimaryNetId();
					UE_LOG(LogParty, VeryVerbose, TEXT("Sending rep data update for member within party [%s]."), *OwnerParty->ToDebugString());
					PartyInterface->UpdatePartyMemberData(*LocalUserId, OwnerParty->GetPartyId(), DefaultPartyDataNamespace, ReplicationPayload);
				}
				else
				{
					UE_LOG(LogParty, Warning, TEXT("Sending rep data update for member within party [%s] Could not identidy owner."), *OwnerParty->ToDebugString());
					PartyInterface->UpdatePartyMemberData(*LocalUserId, OwnerParty->GetPartyId(), DefaultPartyDataNamespace, ReplicationPayload);
				}				
			}
		}
	}
}