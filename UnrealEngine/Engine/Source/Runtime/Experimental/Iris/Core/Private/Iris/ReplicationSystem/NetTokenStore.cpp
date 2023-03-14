// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetTokenStore.h"
#include "Iris/ReplicationSystem/NetTokenStoreState.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializationContext.h"

namespace UE::Net
{

FNetTokenDataStore::FNetTokenDataStore()
: TypeId(FNetToken::InvalidTokenTypeId)
{
}

FNetTokenDataStore::~FNetTokenDataStore()
{
}

FNetTokenStoreKey FNetTokenDataStore::GetTokenKey(FNetToken Token, const FNetTokenStoreState& TokenStoreState) const
{
	if (Token.GetIndex() < (uint32)TokenStoreState.TokenInfos.Num())
	{
		const FNetTokenStoreKey& Key = TokenStoreState.TokenInfos[Token.GetIndex()];
		if (ensureAlwaysMsgf(GetTypeId() == Key.GetTypeId(), TEXT("Cannot resolve NetToken %s with TypeId: %u in DataStore with TypeId: %u"), *Token.ToString(), Key.GetTypeId(), GetTypeId()))
		{
			return Key;
		}
	}

	return FNetTokenStoreKey();
}

FNetToken FNetTokenDataStore::CreateToken(FNetTokenStoreKey Key, FNetTokenStoreState& TokenStoreState)
{
	const uint32 NextTokenIndex = TokenStoreState.TokenInfos.Num();

	if (!ensure(NextTokenIndex < FNetToken::MaxNetTokenCount))
	{
		return FNetToken();
	}

	// Store token info
	TokenStoreState.TokenInfos.Add(Key);

	return FNetToken::MakeNetToken(NextTokenIndex);
}

FNetToken ReadNetToken(FNetBitStreamReader* Reader)
{
	uint32 Index = ReadPackedUint32(Reader);
	
	if (!Reader->IsOverflown())
	{
		return FNetToken(Index);
	}
	else
	{
		return FNetToken();
	}
}

void WriteNetToken(FNetBitStreamWriter* Writer, FNetToken Token)
{
	WritePackedUint32(Writer, Token.GetIndex());
}

FNetTokenStore::FNetTokenStore()
: LocalNetTokenStoreState(new FNetTokenStoreState)
{
}

FNetTokenStore::~FNetTokenStore()
{
	delete LocalNetTokenStoreState;
}

bool FNetTokenStore::RegisterDataStore(FNetTokenDataStore* DataStore)
{
	if (TokenDataStores.Num() >= FNetToken::MaxTypeIdCount)
	{
		return false;
	}

	if (!DataStore)
	{
		return false;
	}

	if (!ensure(DataStore->GetTypeId() == FNetToken::InvalidTokenTypeId))
	{
		// Already registered
		return false;
	}

	DataStore->TypeId = TokenDataStores.Num();
	TokenDataStores.Add(DataStore);

	return true;
}

void FNetTokenStore::WriteTokenData(FNetSerializationContext& Context, const FNetToken& NetToken) const
{
	if (NetToken.IsValid())
	{
		WriteTokenDataForIndex(Context, NetToken.GetIndex());
	}
}

void FNetTokenStore::ReadTokenData(FNetSerializationContext& Context, const FNetToken& NetToken, FNetTokenStoreState& RemoteNetTokenStoreState)
{
	if (NetToken.IsValid())
	{
		// TODO: Guard this better, a map might be better after-all!
		// Also need to protect this if we process inbound data in parallel.
		if (!RemoteNetTokenStoreState.ReserveTokenCount(NetToken.GetIndex() + 1))
		{
			Context.GetBitStreamReader()->DoOverflow();
			return;
		}

		ReadTokenDataForIndex(Context, NetToken.GetIndex(), RemoteNetTokenStoreState);
	}
}

void FNetTokenStore::WriteTokenDataForIndex(FNetSerializationContext& Context, uint32 TokenIndex) const
{
	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

	// Write type
	const FNetTokenStoreKey& TokenKey = LocalNetTokenStoreState->TokenInfos[TokenIndex];
	Writer->WriteBits(TokenKey.TypeId, FNetToken::TokenTypeIdBits);

	// Write token data
	TokenDataStores[TokenKey.TypeId]->WriteTokenData(Context, TokenKey);
}

void FNetTokenStore::ReadTokenDataForIndex(FNetSerializationContext& Context, uint32 TokenIndex, FNetTokenStoreState& TokenStoreState)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	// Read type
	FNetToken::FTypeId TokenTypeId = (FNetToken::FTypeId)Reader->ReadBits(FNetToken::TokenTypeIdBits);

	// Validate that we managed to read and verify the type
	if (Reader->IsOverflown() || TokenTypeId >= (uint32)TokenDataStores.Num() )
	{
		Reader->DoOverflow();
		return;
	}

	const FNetTokenStoreKey StoreKey = TokenDataStores[TokenTypeId]->ReadTokenData(Context);

	// Validate that we managed to read the data for the key
	if (Reader->IsOverflown() || !StoreKey.IsValid())
	{
		Reader->DoOverflow();
		return;
	}

	// Since the same tokendata might be exported multiple times we better validate that it is the same data
	const FNetTokenStoreKey& ExistingStoreKey = TokenStoreState.TokenInfos[TokenIndex];
	if (!ensureAlways(!ExistingStoreKey.IsValid() || StoreKey == ExistingStoreKey))
	{
		Reader->DoOverflow();
		return;
	}
	
	// Store
	TokenStoreState.TokenInfos[TokenIndex] = StoreKey;
}

}
