// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/StringTokenStore.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Hash/CityHash.h"

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define UE_NET_ENABLE_STRING_TOKEN_LOG 0
#else
#	define UE_NET_ENABLE_STRING_TOKEN_LOG 1
#endif 

#if UE_NET_ENABLE_STRING_TOKEN_LOG
#	define UE_LOG_STRINGTOKEN(Format, ...)  UE_LOG(LogIris, Verbose, Format, ##__VA_ARGS__)
#else
#	define UE_LOG_STRINGTOKEN(...)
#endif

#define UE_LOG_STRINGTOKEN_WARNING(Format, ...)  UE_LOG(LogIris, Warning, Format, ##__VA_ARGS__)

namespace UE::Net
{

FNetToken FStringTokenStore::GetOrCreateToken(const TCHAR* Name, uint32 Length)
{
	FNetTokenStoreKey Key = GetOrCreatePersistentString(Name, Length);
	if (Key.IsValid())
	{
		const uint32 StringIndex = Key.GetKeyValue();
		if (StoredTokens[StringIndex].IsValid())
		{
			return StoredTokens[StringIndex];
		}
		else
		{
			const FNetToken NewToken = CreateToken(MakeNetTokenStoreKey(GetTypeId(), StringIndex), *TokenStore.GetLocalNetTokenStoreState());
			StoredTokens[StringIndex] = NewToken;

			UE_LOG_STRINGTOKEN(TEXT("FStringTokenStore::GetOrCreateToken - Created new token %u for %s"), NewToken.GetIndex(), Name);

			return NewToken;
		}
	}

	return FNetToken();
}

FNetToken FStringTokenStore::GetOrCreateToken(const FString& String)
{
	return GetOrCreateToken(ToCStr(String), String.Len());
}

FNetTokenStoreKey FStringTokenStore::GetOrCreatePersistentString(const TCHAR* Name, uint32 Length)
{
	// Hash name
	const uint32 NameSize = Length * sizeof(TCHAR);
	uint64 HashedName = CityHash64((const char*)Name, NameSize);
	
	// Lock if we have to.. make that a policy
	//FScopeLock Lock(&CriticalSection);
	if (const FNetTokenStoreKey* ExistingKey = HashToKey.Find(HashedName))
	{
		return *ExistingKey;
	}
	else if (StoredTokens.Num() < FNetToken::MaxNetTokenCount)
	{
		// Allocate memory and copy persistent string
		TCHAR* PersistentString = (TCHAR*)Allocator.Alloc(NameSize + sizeof(TCHAR), alignof(TCHAR));
		FCString::Strncpy((TCHAR*)PersistentString, Name, Length + 1U);

		// Store bookkeeping data
		const FNetTokenStoreKey NewKey = MakeNetTokenStoreKey(GetTypeId(), StoredStrings.Num());

		HashToKey.Add(HashedName, NewKey);
		StoredStrings.Add(PersistentString);
		StoredTokens.Add(FNetToken());

		return NewKey;
	}

	return FNetTokenStoreKey();
}

FStringTokenStore::FStringTokenStore(FNetTokenStore& InTokenStore)
: TokenStore(InTokenStore)
, Allocator()
{
	// Reserve 0
	StoredStrings.Add(nullptr);
	StoredTokens.Add(FNetToken());

	// Register
	TokenStore.RegisterDataStore(this);
}

const TCHAR* FStringTokenStore::ResolveToken(FNetToken Token) const
{
	return ResolveRemoteToken(Token, *TokenStore.GetLocalNetTokenStoreState());
}

const TCHAR* FStringTokenStore::ResolveRemoteToken(FNetToken Token, const FNetTokenStoreState& NetTokenStoreState) const
{
	const FNetTokenStoreKey StoreKey = GetTokenKey(Token, NetTokenStoreState);

	if (StoreKey.IsValid() && StoreKey.GetKeyValue() < (uint32)StoredStrings.Num())
	{
		return StoredStrings[StoreKey.GetKeyValue()];
	}

	return nullptr;
}

void FStringTokenStore::WriteTokenData(FNetSerializationContext& Context, FNetTokenStoreKey TokenStoreKey) const
{
	// $TODO: $IRIS: Do not calculate the length of the string to write the data.
	WriteString(Context.GetBitStreamWriter(), FStringView(StoredStrings[TokenStoreKey.GetKeyValue()]));
}

FNetTokenStoreKey FStringTokenStore::ReadTokenData(FNetSerializationContext& Context)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	// Read the token data and add it to the string store without assigning LocalToken
	FString Temp;
	ReadString(Reader, Temp);

	if (!Reader->IsOverflown())
	{
		return GetOrCreatePersistentString(*Temp, Temp.Len());
	}
	else
	{
		return FNetTokenStoreKey();
	}
}

}
