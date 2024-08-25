// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AsyncTestStep.h"
#include "Online/Presence.h"
using namespace UE::Online;

TSharedRef<FUserPresence> GetGenericPresenceA(FAccountId AccountId)
{
	TSharedRef<FUserPresence> Presence = MakeShared<FUserPresence>();

	Presence->AccountId = AccountId;
	Presence->GameStatus = EUserPresenceGameStatus::PlayingThisGame;
	Presence->Joinability = EUserPresenceJoinability::InviteOnly;
	Presence->RichPresenceString = TEXT("[DEBUGSTR]");
	Presence->Status = EUserPresenceStatus::Online;
	Presence->StatusString = TEXT("Online");
	Presence->Properties.Add(TEXT("[DEBUGKEY1]"), TEXT("[DEBUGVALUE1]"));

	return Presence;
}

TSharedRef<FUserPresence> GetGenericPresenceB(FAccountId AccountId)
{
	TSharedRef<FUserPresence> Presence = MakeShared<FUserPresence>();

	Presence->AccountId = AccountId;
	Presence->GameStatus = EUserPresenceGameStatus::PlayingOtherGame;
	Presence->Joinability = EUserPresenceJoinability::Private;
	Presence->RichPresenceString = TEXT("[DEBUGSTR2]");
	Presence->Status = EUserPresenceStatus::ExtendedAway;
	Presence->StatusString = TEXT("Asleep");
	Presence->Properties.Add(TEXT("[DEBUGKEY2]"), TEXT("[DEBUGVALUE2]"));

	return Presence;
}

FPartialUpdatePresence::Params GetGenericPresenceMutationAtoB(FAccountId AccountId)
{
	FPartialUpdatePresence::Params Params;

	Params.LocalAccountId = AccountId;
	Params.Mutations.GameStatus = EUserPresenceGameStatus::PlayingOtherGame;
	Params.Mutations.Joinability = EUserPresenceJoinability::Private;
	Params.Mutations.RichPresenceString = TEXT("[DEBUGSTR2]");
	Params.Mutations.Status = EUserPresenceStatus::ExtendedAway;
	Params.Mutations.StatusString = TEXT("Asleep");
	Params.Mutations.RemovedProperties.Add(TEXT("[DEBUGKEY1]"));
	Params.Mutations.UpdatedProperties.Add(TEXT("[DEBUGKEY2]"), TEXT("[DEBUGVALUE2]"));

	return Params;
}

void CheckPresenceAreEqual(const FUserPresence& Presence1, const FUserPresence& Presence2)
{
	CHECK(Presence1.AccountId == Presence2.AccountId);
	CHECK(Presence1.GameStatus == Presence2.GameStatus);
	CHECK(Presence1.Joinability == Presence2.Joinability);
	CHECK(Presence1.RichPresenceString == Presence2.RichPresenceString);
	CHECK(Presence1.Status == Presence2.Status);
	CHECK(Presence1.StatusString == Presence2.StatusString);
	CHECK(Presence1.Properties.Num() == Presence2.Properties.Num());

	for (const TPair<FString, FPresenceVariant>& Pair : Presence1.Properties)
	{
		CHECK(Presence2.Properties.Contains(Pair.Key));
		if (Presence2.Properties.Contains(Pair.Key))
		{
			CHECK(Pair.Value == *Presence2.Properties.Find(Pair.Key));
		}
	}
}

// don't check user ids here- may be used to check for an update on a specific user
void CheckPresencesNotEqual(const FUserPresence& Presence1, const FUserPresence& Presence2)
{
	CHECK(Presence1.GameStatus != Presence2.GameStatus);
	CHECK(Presence1.Joinability != Presence2.Joinability);
	CHECK(Presence1.RichPresenceString != Presence2.RichPresenceString);
	CHECK(Presence1.Status != Presence2.Status);
	CHECK(Presence1.StatusString != Presence2.StatusString);

	for (const TPair<FString, FPresenceVariant>& Pair : Presence1.Properties)
	{
		if (Presence2.Properties.Contains(Pair.Key))
		{
			CHECK(Pair.Value != *Presence2.Properties.Find(Pair.Key));
		}
	}
}

// Presence2 is static because this is intended for the first passed in presence to be something queried and the second value to be the static presence taht it is expected to be
class FComparePresencesHelper : public FAsyncTestStep
{
public:
	FComparePresencesHelper(TSharedPtr<const FUserPresence>& InPresence1, TSharedRef<const FUserPresence> InPresence2, bool inBShouldBeEqual = true)
		: Presence1(InPresence1)
		, Presence2(InPresence2)
		, bShouldBeEqual(inBShouldBeEqual)
	{

	}
	
	virtual void Run(FAsyncStepResult Promise, SubsystemType Services) override
	{
		CHECK(Presence1.IsValid());

		if (Presence1.IsValid())
		{
			if (bShouldBeEqual)
			{
				CheckPresenceAreEqual(*Presence1, *Presence2);
			}
			else
			{
				CheckPresencesNotEqual(*Presence1, *Presence2);
			}
		}

		Promise->SetValue(true);
	}

protected:
	TSharedPtr<const FUserPresence>& Presence1;
	TSharedRef<const FUserPresence> Presence2;
	bool bShouldBeEqual;
};