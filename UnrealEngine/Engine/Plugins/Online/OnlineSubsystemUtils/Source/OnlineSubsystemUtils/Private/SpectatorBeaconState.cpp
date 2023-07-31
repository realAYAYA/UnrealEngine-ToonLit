// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpectatorBeaconState.h"
#include "OnlineBeacon.h"
#include "OnlineSubsystemTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SpectatorBeaconState)

DEFINE_LOG_CATEGORY(LogSpectatorBeacon);

bool FSpectatorReservation::IsValid() const
{
	bool bIsValid = false;
	if (SpectatorId.IsValid())
	{
		bIsValid = true;
		if (!Spectator.UniqueId.IsValid())
		{
			bIsValid = false;
		}
		else if (Spectator.Platform.IsEmpty())
		{
			bIsValid = false;
		}
		else if (SpectatorId == Spectator.UniqueId &&
				Spectator.ValidationStr.IsEmpty())
		{
				bIsValid = false;
		
		}
	}

	return bIsValid;
}

void FSpectatorReservation::Dump() const
{
	UE_LOG(LogSpectatorBeacon, Display, TEXT("Spectator Reservation:"));
	UE_LOG(LogSpectatorBeacon, Display, TEXT("  Spectator: %s"), *SpectatorId.ToDebugString());
	UE_LOG(LogSpectatorBeacon, Display, TEXT("  UniqueId: %s"), *Spectator.UniqueId.ToDebugString());
	UE_LOG(LogSpectatorBeacon, Display, TEXT("	Crossplay: %s"), *LexToString(Spectator.bAllowCrossplay));
#if UE_BUILD_SHIPPING
		UE_LOG(LogSpectatorBeacon, Display, TEXT("  ValidationStr: %d bytes"), Spectator.ValidationStr.Len());
#else
		UE_LOG(LogSpectatorBeacon, Display, TEXT("  ValidationStr: %s"), *Spectator.ValidationStr);
#endif
		UE_LOG(LogSpectatorBeacon, Display, TEXT("  ElapsedTime: %0.2f"), Spectator.ElapsedTime);
}

USpectatorBeaconState::USpectatorBeaconState(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	SessionName(NAME_None),
	NumConsumedReservations(0),
	MaxReservations(0),
	bRestrictCrossConsole(false)
{
}

bool USpectatorBeaconState::InitState(int32 InMaxReservations, FName InSessionName)
{
	if (InMaxReservations > 0)
	{
		SessionName = InSessionName;
		MaxReservations = InMaxReservations;
		Reservations.Empty(MaxReservations);
		return true;
	}

	return false;
}

bool USpectatorBeaconState::HasCrossplayOptOutReservation() const
{
	for (const FSpectatorReservation& ExistingReservation : Reservations)
	{
		const FPlayerReservation& ExistingPlayer = ExistingReservation.Spectator;
		if (!ExistingPlayer.bAllowCrossplay)
		{
			return true;
		}
	}

	return false;
}

int32 USpectatorBeaconState::GetReservationPlatformCount(const FString& InPlatform) const
{
	int32 PlayerCount = 0;
	for (const FSpectatorReservation& ExistingReservation : Reservations)
	{
		const FPlayerReservation& ExistingPlayer = ExistingReservation.Spectator;
		if (ExistingPlayer.Platform == InPlatform)
		{
			PlayerCount++;
		}
	}

	return PlayerCount;
}

bool USpectatorBeaconState::CrossPlayAllowed(const FSpectatorReservation& ReservationRequest) const
{
	// Since this player is a spectator, it won't be playing, so allow crossplay.
	return true;
}

bool USpectatorBeaconState::DoesReservationFit(const FSpectatorReservation& ReservationRequest) const
{
	const bool bRoomForReservation = (NumConsumedReservations + 1) <= MaxReservations;

	UE_LOG(LogSpectatorBeacon, Verbose, TEXT("USpectatorBeaconState::DoesReservationFit: NumConsumedReservations: %d MaxReservations: %d"), NumConsumedReservations, MaxReservations);

	return bRoomForReservation;
}

bool USpectatorBeaconState::AddReservation(const FSpectatorReservation& ReservationRequest)
{
	if (UE_LOG_ACTIVE(LogSpectatorBeacon, Verbose))
	{
		UE_LOG(LogSpectatorBeacon, Verbose, TEXT("USpectatorBeaconState::AddReservation"));
		ReservationRequest.Dump();
	}
	
	NumConsumedReservations += 1;
	UE_LOG(LogSpectatorBeacon, Verbose, TEXT("UsPECTATORBeaconState::AddReservation: Setting NumConsumedReservations to %d"), NumConsumedReservations);
	int32 ResIdx = Reservations.Add(ReservationRequest);
	SanityCheckReservations(false);

	return true;
}

bool USpectatorBeaconState::RemoveReservation(const FUniqueNetIdRepl& Spectator)
{
	const int32 ExistingReservationIdx = GetExistingReservation(Spectator);
	if (ExistingReservationIdx != INDEX_NONE)
	{
		NumConsumedReservations -= 1;
		UE_LOG(LogSpectatorBeacon, Verbose, TEXT("USpectatorBeaconState::RemoveReservation: %s, setting NumConsumedReservations to %d"), *Spectator.ToString(), NumConsumedReservations);

		const FPlayerReservation& PlayerRes = Reservations[ExistingReservationIdx].Spectator;
		FUniqueNetIdMatcher PlayerMatch(*PlayerRes.UniqueId);
		int32 FoundIdx = PlayersPendingJoin.IndexOfByPredicate(PlayerMatch);
		if (FoundIdx != INDEX_NONE)
		{
			PlayersPendingJoin.RemoveAtSwap(FoundIdx);
		}

		Reservations.RemoveAtSwap(ExistingReservationIdx);

		SanityCheckReservations(false);
		return true;
	}

	return false;
}

void USpectatorBeaconState::RegisterAuthTicket(const FUniqueNetIdRepl& InSpectatorId, const FString& InAuthTicket)
{
	if (InSpectatorId.IsValid() && !InAuthTicket.IsEmpty())
	{
		bool bFoundReservation = false;

		for (int32 ResIdx = 0; ResIdx < Reservations.Num() && !bFoundReservation; ResIdx++)
		{
			FSpectatorReservation& ReservationEntry = Reservations[ResIdx];

			FPlayerReservation& PlayerRes = ReservationEntry.Spectator;

			if (InSpectatorId == PlayerRes.UniqueId)
			{
				if (PlayerRes.ValidationStr.IsEmpty())
				{
					UE_LOG(LogSpectatorBeacon, VeryVerbose, TEXT("Setting auth ticket for spectator %s."), *InSpectatorId.ToDebugString());
				}
				else if (PlayerRes.ValidationStr != InAuthTicket)
				{
					UE_LOG(LogSpectatorBeacon, Warning, TEXT("Auth ticket changing for spectator %s."), *InSpectatorId.ToDebugString());
				}

				PlayerRes.ValidationStr = InAuthTicket;
				bFoundReservation = true;
				break;
			}
		}

		if (!bFoundReservation)
		{
			UE_LOG(LogSpectatorBeacon, Warning, TEXT("Found no reservation for player %s, while registering auth ticket."), *InSpectatorId.ToDebugString());
		}
	}
}


bool USpectatorBeaconState::RemovePlayer(const FUniqueNetIdRepl& PlayerId)
{
	UE_LOG(LogSpectatorBeacon, VeryVerbose, TEXT("USpectatrBeaconState::RemovePlayer: %s"), *PlayerId.ToDebugString());
	bool bWasRemoved = false;

	for (int32 ResIdx = 0; ResIdx < Reservations.Num() && !bWasRemoved; ResIdx++)
	{
		FSpectatorReservation& Reservation = Reservations[ResIdx];
		FPlayerReservation& PlayerEntry = Reservation.Spectator;
		if (PlayerEntry.UniqueId == PlayerId)
		{
			bWasRemoved = true;

			// free up a consumed entry
			NumConsumedReservations--;
			if (UE_LOG_ACTIVE(LogSpectatorBeacon, Verbose))
			{
				UE_LOG(LogSpectatorBeacon, Verbose, TEXT("USpectatorBeaconState::RemovePlayer: Player found in reservation with id %s, setting NumConsumedReservations to %d"), *Reservation.SpectatorId.ToString(), NumConsumedReservations);
				Reservation.Dump();
			}
			SanityCheckReservations(true);
			UE_LOG(LogSpectatorBeacon, Verbose, TEXT("USpectatorBeaconState::RemovePlayer: Empty reservation found with spectator %s, removing"), *Reservation.SpectatorId.ToString());
			Reservations.RemoveAtSwap(ResIdx--);
		}
	}

	SanityCheckReservations(false);
	return bWasRemoved;
}

int32 USpectatorBeaconState::GetExistingReservation(const FUniqueNetIdRepl& Spectator) const
{
	int32 Result = INDEX_NONE;
	for (int32 ResIdx = 0; ResIdx < Reservations.Num(); ResIdx++)
	{
		const FSpectatorReservation& ReservationEntry = Reservations[ResIdx];
		if (ReservationEntry.SpectatorId == Spectator)
		{
			Result = ResIdx;
			break;
		}
	}

	return Result;
}

bool USpectatorBeaconState::UpdateMemberPlatform(const FUniqueNetIdRepl& Spectator, const FString& PlatformName)
{
	for (int32 ResIdx = 0; ResIdx < Reservations.Num(); ResIdx++)
	{
		FSpectatorReservation& ReservationEntry = Reservations[ResIdx];
		FPlayerReservation& PlayerReservation = ReservationEntry.Spectator;
		if (PlayerReservation.UniqueId == Spectator)
		{
			if (!PlatformName.IsEmpty())
			{
				PlayerReservation.Platform = PlatformName;
			}
			// Return that member was updated
			return true;
		}
	}

	//Return that member was not updated
	return false;
}

bool USpectatorBeaconState::PlayerHasReservation(const FUniqueNetId& PlayerId) const
{
	bool bFound = false;

	for (int32 ResIdx = 0; ResIdx < Reservations.Num(); ResIdx++)
	{
		const FSpectatorReservation& ReservationEntry = Reservations[ResIdx];
		if (*ReservationEntry.Spectator.UniqueId == PlayerId)
		{
			bFound = true;
			break;
		}
	}

	return bFound;
}

bool USpectatorBeaconState::GetPlayerValidation(const FUniqueNetId& PlayerId, FString& OutValidation) const
{
	bool bFound = false;
	OutValidation = FString();

	for (int32 ResIdx = 0; ResIdx < Reservations.Num() && !bFound; ResIdx++)
	{
		const FSpectatorReservation& ReservationEntry = Reservations[ResIdx];
		if (*ReservationEntry.Spectator.UniqueId == PlayerId)
		{
			OutValidation = ReservationEntry.Spectator.ValidationStr;
			bFound = true;
			break;
		}
	}

	return bFound;
}

void USpectatorBeaconState::DumpReservations() const
{
	FUniqueNetIdRepl NetId;
	FPlayerReservation PlayerRes;

	UE_LOG(LogSpectatorBeacon, Display, TEXT("Session that reservations are for: %s"), *SessionName.ToString());
	UE_LOG(LogSpectatorBeacon, Display, TEXT("Number total reservations: %d"), MaxReservations);
	UE_LOG(LogSpectatorBeacon, Display, TEXT("Number consumed reservations: %d"), NumConsumedReservations);
	UE_LOG(LogSpectatorBeacon, Display, TEXT("Number of spectator reservations: %d"), Reservations.Num());

	// Log each spectator that has a reservation
	for (int32 ResIndex = 0; ResIndex < Reservations.Num(); ResIndex++)
	{
		NetId = Reservations[ResIndex].SpectatorId;
		UE_LOG(LogSpectatorBeacon, Display, TEXT("\t Spectator: %s"), *NetId->ToDebugString());
		PlayerRes = Reservations[ResIndex].Spectator;
		UE_LOG(LogSpectatorBeacon, Display, TEXT("\t  Member: %s [%s] Cross: %s"), *PlayerRes.UniqueId->ToString(), *PlayerRes.Platform, *LexToString(PlayerRes.bAllowCrossplay));
	}
	UE_LOG(LogSpectatorBeacon, Display, TEXT(""));
}

void USpectatorBeaconState::SanityCheckReservations(const bool bIgnoreEmptyReservations) const
{
#if !UE_BUILD_SHIPPING
	// Verify that each player is only in exactly one reservation
	TMap<FUniqueNetIdRepl, FUniqueNetIdRepl> PlayersInReservation;
	for (const FSpectatorReservation& Reservation : Reservations)
	{
		if (!Reservation.SpectatorId.IsValid())
		{
			DumpReservations();
			checkf(false, TEXT("Reservation does not have valid spectator!"));
		}
		const FPlayerReservation& PlayerReservation = Reservation.Spectator;
		if (PlayerReservation.UniqueId.IsValid())
		{
			const FUniqueNetIdRepl* const ExistingReservationLeader = PlayersInReservation.Find(PlayerReservation.UniqueId);
			if (ExistingReservationLeader != nullptr)
			{
				DumpReservations();
				checkf(false, TEXT("Player %s is in multiple reservations!"), *PlayerReservation.UniqueId.ToString());
			}
			PlayersInReservation.Add(PlayerReservation.UniqueId, Reservation.SpectatorId);
		}
	}
#endif // !UE_BUILD_SHIPPING
}

