// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPAddressSteam.h"
#include "Algo/Reverse.h"

TArray<uint8> FInternetAddrSteam::GetRawIp() const
{
	TArray<uint8> RawAddressArray;
	const uint8* SteamIdWalk = SteamId->GetBytes();
	while (RawAddressArray.Num() < SteamId->GetSize())
	{
		RawAddressArray.Add(*SteamIdWalk);
		++SteamIdWalk;
	}

	// We want to make sure that these arrays are in big endian format.
#if PLATFORM_LITTLE_ENDIAN
	Algo::Reverse(RawAddressArray);
#endif

	return RawAddressArray;
}

void FInternetAddrSteam::SetRawIp(const TArray<uint8>& RawAddr)
{
	uint64 NewSteamId = 0;

	// Make a quick copy of the array
	TArray<uint8> WorkingArray = RawAddr;

	// Flip the entire array.
	// Normally we would just do a ntohll on the final result but it's not a portable function.
#if PLATFORM_LITTLE_ENDIAN
	Algo::Reverse(WorkingArray);
#endif

	for (int32 i = 0; i < WorkingArray.Num(); ++i)
	{
		NewSteamId |= (uint64)WorkingArray[i] << (i * 8);
	}

	SteamId = FUniqueNetIdSteam::Create(NewSteamId);
}

/**
 * Sets the ip address from a string ("steam.STEAMID" or "STEAMID")
 *
 * @param InAddr the string containing the new ip address to use
 * @param bIsValid was the address valid for use
 */
void FInternetAddrSteam::SetIp(const TCHAR* InAddr, bool& bIsValid)
{
	bIsValid = false;
	FString InAddrStr(InAddr);

	// Check for the steam. prefix
	FString SteamIPAddrStr;
	if (InAddrStr.StartsWith(STEAM_URL_PREFIX))
	{
		SteamIPAddrStr = InAddrStr.Mid(UE_ARRAY_COUNT(STEAM_URL_PREFIX) - 1);
	}
	else
	{
		SteamIPAddrStr = InAddrStr;
	}

	// Resolve the address/port
	FString SteamIPStr, SteamChannelStr;
	if (SteamIPAddrStr.Split(":", &SteamIPStr, &SteamChannelStr, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		const uint64 Id = FCString::Atoi64(*SteamIPStr);
		if (Id != 0)
		{
			SteamId = FUniqueNetIdSteam::Create(Id);
			const int32 Channel = FCString::Atoi(*SteamChannelStr);
			if (Channel != 0 || SteamChannelStr == "0")
			{
				SteamChannel = Channel;
				bIsValid = true;
			}
		}
	}
	else
	{
		const uint64 Id = FCString::Atoi64(*SteamIPAddrStr);
		if (Id != 0)
		{
			SteamId = FUniqueNetIdSteam::Create(Id);
			bIsValid = true;
		}

		SteamChannel = 0;
	}

	bIsValid = bIsValid && SteamId->IsValid();
}
