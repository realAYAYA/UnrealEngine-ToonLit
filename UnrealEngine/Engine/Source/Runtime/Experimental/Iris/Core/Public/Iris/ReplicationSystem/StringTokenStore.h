// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "Iris/ReplicationSystem/NetTokenStore.h"
#include "Misc/MemStack.h"

namespace UE::Net
{

// Simple token store used to store string tokens
// When the PackageMapRefactor is complete we will most likely rely on NetTagManager for persistent storage
class FStringTokenStore : public FNetTokenDataStore
{
	UE_NONCOPYABLE(FStringTokenStore);
public:
	explicit FStringTokenStore(FNetTokenStore& TokenStore);

	// Create a string token for the provided string
	IRISCORE_API FNetToken GetOrCreateToken(const FString& String);
	IRISCORE_API FNetToken GetOrCreateToken(const TCHAR* Name, uint32 Length);

	// Resolve a local token
	IRISCORE_API const TCHAR* ResolveToken(FNetToken Token) const;

	// Resolve a token received from remote
	IRISCORE_API const TCHAR* ResolveRemoteToken(FNetToken Token, const FNetTokenStoreState& NetTokenStoreState) const;

protected:
	// Serialize data for a token, note there is not validation in this function
	virtual void WriteTokenData(FNetSerializationContext& Context, FNetTokenStoreKey TokenStoreKey) const override;

	// Read data for a token, returns a valid StoreKey if successful read
	virtual FNetTokenStoreKey ReadTokenData(FNetSerializationContext& Context) override;

	// Create a persistent string
	FNetTokenStoreKey GetOrCreatePersistentString(const TCHAR* Name, uint32 Length);

private:
	FNetTokenStore& TokenStore;
	TMap<uint64, FNetTokenStoreKey> HashToKey;
	TArray<const TCHAR*> StoredStrings;
	TArray<FNetToken> StoredTokens;
	FMemStackBase Allocator;
};

}
