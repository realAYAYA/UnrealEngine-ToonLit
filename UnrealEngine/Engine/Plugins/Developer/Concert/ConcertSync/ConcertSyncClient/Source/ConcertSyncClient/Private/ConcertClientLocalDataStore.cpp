// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertClientLocalDataStore.h"
#include "ConcertDataStore.h"

FConcertClientLocalDataStore::FConcertClientLocalDataStore()
	: DataStore(MakeUnique<FConcertDataStore>())
{
}

FConcertDataStoreResult FConcertClientLocalDataStore::InternalFetch(const FName& Key, const FName& TypeName) const
{
	return DataStore->Fetch(Key, TypeName);
}

FConcertDataStoreResult FConcertClientLocalDataStore::InternalStore(const FName& Key, const FName& Typename, const FConcertSessionSerializedPayload& Value)
{
	return DataStore->Store(Key, Typename, Value);
}
