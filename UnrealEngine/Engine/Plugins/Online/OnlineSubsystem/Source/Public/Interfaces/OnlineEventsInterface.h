// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineKeyValuePair.h"

ONLINESUBSYSTEM_API DECLARE_LOG_CATEGORY_EXTERN(LogOnlineEvents, Log, All);
#define UE_LOG_ONLINE_EVENTS(Verbosity, Format, ...) \
{ \
	UE_LOG(LogOnlineEvents, Verbosity, TEXT("%s%s"), ONLINE_LOG_PREFIX, *FString::Printf(Format, ##__VA_ARGS__)); \
}

#define UE_CLOG_ONLINE_EVENTS(Conditional, Verbosity, Format, ...) \
{ \
	UE_CLOG(Conditional, LogOnlineEvents, Verbosity, TEXT("%s%s"), ONLINE_LOG_PREFIX, *FString::Printf(Format, ##__VA_ARGS__)); \
}

class FUniqueNetId;
// Temporary class to assist in deprecation of changing the key from FName to FString. This will eventually be a typedef to FOnlineKeyValuePairs< FString, FVariantData >
class FOnlineEventParms : public FOnlineKeyValuePairs< FString, FVariantData >
{
	typedef FOnlineKeyValuePairs<FString, FVariantData> Super;

public:
	FORCEINLINE FOnlineEventParms() {}
	FORCEINLINE FOnlineEventParms(FOnlineEventParms&& Other) : Super(MoveTemp(Other)) {}
	FORCEINLINE FOnlineEventParms(const FOnlineEventParms& Other) : Super(Other) {}
	FORCEINLINE FOnlineEventParms& operator=(FOnlineEventParms&& Other) { Super::operator=(MoveTemp(Other)); return *this; }
	FORCEINLINE FOnlineEventParms& operator=(const FOnlineEventParms& Other) { Super::operator=(Other); return *this; }

public:
	UE_DEPRECATED(4.26, "FOnlineEventParms now uses FString for the key type instead of FName")
		FOnlineEventParms(const FOnlineKeyValuePairs< FName, FVariantData >& DeprecatedValues)
	{
		Reserve(DeprecatedValues.Num());
		for (const TPair< FName, FVariantData >& DeprecatedValue : DeprecatedValues)
		{
			Super::Emplace(DeprecatedValue.Key.ToString(), DeprecatedValue.Value);
		}
	}

	UE_DEPRECATED(4.26, "FOnlineEventParms now uses FString for the key type instead of FName")
		FOnlineEventParms(FOnlineKeyValuePairs< FName, FVariantData >&& DeprecatedValues)
	{
		Reserve(DeprecatedValues.Num());
		for (TPair< FName, FVariantData >& DeprecatedValue : DeprecatedValues)
		{
			Super::Emplace(DeprecatedValue.Key.ToString(), MoveTemp(DeprecatedValue.Value));
		}
	}
 	using Super::Add;
 	UE_DEPRECATED(4.26, "FOnlineEventParms now uses FString for the key type instead of FName")
 		FVariantData& Add(const FName& InKey, const FVariantData& InValue)
 	{
 		return Super::Add(InKey.ToString(), InValue);
 	}

	using Super::Emplace;
	UE_DEPRECATED(4.26, "FOnlineEventParms now uses FString for the key type instead of FName")
		FVariantData& Emplace(const FName& InKey, const FVariantData& InValue)
	{
		return Super::Emplace(InKey.ToString(), InValue);
	}

	using Super::Find;
	UE_DEPRECATED(4.26, "FOnlineEventParms now uses FString for the key type instead of FName")
		FVariantData* Find(const FName& InKey)
	{
		return Super::Find(InKey.ToString());
	}
};
// typedef FOnlineKeyValuePairs< FString, FVariantData > FOnlineEventParms;

/**
 *	IOnlineEvents - Interface class for events
 */
class IOnlineEvents
{
public:

	/**
	 * Trigger an event by name
	 *
	 * @param PlayerId	- Player to trigger the event for
	 * @param EventName - Name of the event
	 * @param Parms		- The parameter list to be passed into the event
	 *
	 * @return Whether the event was successfully triggered
	 */
	virtual bool TriggerEvent( const FUniqueNetId& PlayerId, const TCHAR* EventName, const FOnlineEventParms& Parms ) = 0;

	/**
	 * Quick way to send a valid PlayerSessionId with every event, required for Xbox One
	 *
	 * @param PlayerId the unique id of the player this session is associated with
	 * @param PlayerSessionId A GUID unique to this player session
	 */
	virtual void SetPlayerSessionId(const FUniqueNetId& PlayerId, const FGuid& PlayerSessionId) = 0;
};