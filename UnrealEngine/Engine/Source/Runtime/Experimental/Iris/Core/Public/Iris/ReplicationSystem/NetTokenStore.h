// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "NetToken.h"

class UNetTokenDataStream;
namespace UE::Net
{
	class FNetTokenDataStore;
	class FNetTokenStore;
	class FNetTokenStoreState;
	class FNetSerializationContext;
}

namespace UE::Net
{

// virtual interface for NetTokenDataStores
class FNetTokenDataStore
{
public:
	
	virtual ~FNetTokenDataStore();

protected:
	FNetTokenDataStore();

	virtual void WriteTokenData(FNetSerializationContext& Context, FNetTokenStoreKey Key) const = 0;
	virtual FNetTokenStoreKey ReadTokenData(FNetSerializationContext& Context) = 0;

	FNetTokenStoreKey GetTokenKey(FNetToken Token, const FNetTokenStoreState& TokenStoreState) const;
	inline FNetToken::FTypeId GetTypeId() const { return TypeId; }

	// Create new NetToken
	FNetToken CreateToken(FNetTokenStoreKey Key, FNetTokenStoreState& TokenStoreState);

	// Make NetTokenStoreKey
	static FNetTokenStoreKey MakeNetTokenStoreKey(FNetToken::FTypeId TokenTypeId, uint32 TokenKeyValue);

private:
	friend FNetTokenStore;
	FNetToken::FTypeId TypeId;
};

// This is the token store, we have one per game instance
class FNetTokenStore
{
	UE_NONCOPYABLE(FNetTokenStore);
public:
	FNetTokenStore();
	~FNetTokenStore();

	// Register DataStore and return true if it was registered.
	bool RegisterDataStore(FNetTokenDataStore* DataStore);

	const FNetTokenStoreState* GetLocalNetTokenStoreState() const { return LocalNetTokenStoreState; }
	FNetTokenStoreState* GetLocalNetTokenStoreState() { return LocalNetTokenStoreState; }

	// Write data associated with the NetToken
	void WriteTokenData(FNetSerializationContext& Context, const FNetToken& NetToken) const;

	// Read data associated with the NetToken
	void ReadTokenData(FNetSerializationContext& Context, const FNetToken& NetToken, FNetTokenStoreState& RemoteNetTokenStoreState);

private:

	// Write token data for the TokenIndex
	void WriteTokenDataForIndex(FNetSerializationContext& Context, uint32 TokenIndex) const;

	// Read token data for the TokenIndex
	void ReadTokenDataForIndex(FNetSerializationContext& Context, uint32 TokenIndex, FNetTokenStoreState& Resolver);

private:
	friend UNetTokenDataStream;
	friend FNetTokenDataStore;

	FNetTokenStoreState* LocalNetTokenStoreState;
	TArray<FNetTokenDataStore*> TokenDataStores;
};

inline FNetTokenStoreKey FNetTokenDataStore::MakeNetTokenStoreKey(FNetToken::FTypeId TokenTypeId, uint32 TokenKeyValue)
{
	check(TokenTypeId < FNetToken::MaxTypeIdCount);
	check(TokenKeyValue < FNetToken::MaxNetTokenCount)

	FNetTokenStoreKey TokenKey;
	TokenKey.TypeId = TokenTypeId;
	TokenKey.Key = TokenKeyValue;

	return TokenKey;
}


}
