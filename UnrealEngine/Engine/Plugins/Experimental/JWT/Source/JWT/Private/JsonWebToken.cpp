// Copyright Epic Games, Inc. All Rights Reserved.

#include "JsonWebToken.h"
#include "Algo/Count.h"
#include "Misc/Base64.h"
#include "Serialization/JsonSerializer.h"

// JWT header field names
const TCHAR *const FJsonWebToken::HEADER_TYPE = TEXT("typ");
const TCHAR *const FJsonWebToken::HEADER_KEY_ID = TEXT("kid");
const TCHAR *const FJsonWebToken::HEADER_ALGORITHM = TEXT("alg");

// JWT header expected values
const TCHAR *const FJsonWebToken::TYPE_VALUE_JWT = TEXT("JWT");

// JWT payload claim field names
const TCHAR *const FJsonWebToken::CLAIM_ISSUER = TEXT("iss");
const TCHAR *const FJsonWebToken::CLAIM_ISSUED_AT = TEXT("iat");
const TCHAR *const FJsonWebToken::CLAIM_EXPIRATION = TEXT("exp");
const TCHAR *const FJsonWebToken::CLAIM_SUBJECT = TEXT("sub");
const TCHAR *const FJsonWebToken::CLAIM_AUDIENCE = TEXT("aud");


namespace
{
	/**
	 * Helper for splitting FStringViews, like FString.
	 */
	bool SplitStringView(const FStringView InSourceString, const TCHAR InDelimiter, FStringView& OutLeft, FStringView& OutRight)
	{
		int32 Position = INDEX_NONE;
		if (!InSourceString.FindChar(InDelimiter, Position) || Position == INDEX_NONE)
		{
			return false;
		}

		OutLeft = InSourceString.Left(Position);
		OutRight = InSourceString.RightChop(Position + 1);
		return true;
	}

	/**
	 * Splits an encoded JWT string into the 3 header, payload, and signature parts.
	 */
	bool SplitEncodedJsonWebTokenString(const FStringView InEncodedJsonWebTokenString, FStringView& OutEncodedHeaderPart, FStringView& OutEncodedPayloadPart, FStringView& OutEncodedSignaturePart)
	{
		FStringView Rest;

		if (!SplitStringView(InEncodedJsonWebTokenString, TEXT('.'), OutEncodedHeaderPart, Rest))
		{
			UE_LOG(LogJwt, Warning, TEXT("[SplitEncodedJsonWebTokenString] Cannot extract header from token string."));
			return false;
		}

		if (!SplitStringView(Rest, TEXT('.'), OutEncodedPayloadPart, OutEncodedSignaturePart))
		{
			UE_LOG(LogJwt, Warning, TEXT("[SplitEncodedJsonWebTokenString] Cannot extract payload from token string."));
			return false;
		}

		if (OutEncodedHeaderPart.IsEmpty() || OutEncodedPayloadPart.IsEmpty())
		{
			UE_LOG(LogJwt, Warning, TEXT("[SplitEncodedJsonWebTokenString] Empty header and/or payload in token string."));
			return false;
		}

		return true;
	}

	bool Base64Decode(const FStringView InSource, FString& OutDest)
	{
		if (InSource.IsEmpty())
		{
			return false;
		}

		return FBase64::Decode(FString(InSource), OutDest);
	}

	bool Base64Decode(const FStringView InSource, TArray<uint8>& OutDest)
	{
		if (InSource.IsEmpty())
		{
			return false;
		}

		return FBase64::Decode(FString(InSource), OutDest);
	}

	bool StringViewToBytes(const FStringView In, TArray<uint8>& OutBytes, const bool IsEncoded)
	{
		if (IsEncoded)
		{
			return Base64Decode(In, OutBytes);
		}

		OutBytes.Reserve(In.Len());
		for (const TCHAR& Ch : In)
		{
			OutBytes.Add(static_cast<uint8>(Ch));
		}

		return true;
	}
}

void FJsonWebToken::DumpJsonObject(const FJsonObject& InJsonObject)
{
	if (!UE_LOG_ACTIVE(LogJwt, VeryVerbose))
	{
		return;
	}

	UE_LOG(LogJwt, VeryVerbose, TEXT("size=%d, fields:"), InJsonObject.Values.Num());

	for (const TMap<FString, TSharedPtr<FJsonValue>>::ElementType& Pair : InJsonObject.Values)
	{
		const FString& Name = Pair.Key;
		const TSharedPtr<FJsonValue>& Value = Pair.Value;

		if (!Value)
		{
			UE_LOG(LogJwt, VeryVerbose, TEXT("  %s [undefined]"), *Name);
			continue;
		}

		if (Value->IsNull())
		{
			UE_LOG(LogJwt, VeryVerbose, TEXT("  %s [null]"), *Name);
			continue;
		}

		// Present the JSON value in a printable form; TryGetString() is overridden for
		// non-compound types (bool, number, string).
		FString OutString;
		if (Value->TryGetString(OutString))
		{
			UE_LOG(LogJwt, VeryVerbose, TEXT("  %s: %s"), *Name, *OutString);
			continue;
		}

		const TArray< TSharedPtr<FJsonValue> >* OutArray = nullptr;
		if (Value->TryGetArray(OutArray))
		{
			UE_LOG(LogJwt, VeryVerbose, TEXT("  %s [array, len=%d]"), *Name, OutArray->Num());
			continue;
		}

		const TSharedPtr<FJsonObject>* OutObject = nullptr;
		if (Value->TryGetObject(OutObject))
		{
			const int32 Length = (*OutObject).IsValid() ? (*OutObject)->Values.Num() : 0;

			UE_LOG(LogJwt, VeryVerbose, TEXT("  %s [object, len=%d]"), *Name, Length);
			continue;
		}

		UE_LOG(LogJwt, VeryVerbose, TEXT("  %s [unknown type])"), *Name);
	}
}


TSharedPtr<FJsonObject> FJsonWebToken::FromJson(const FString& InJsonStr)
{
	TSharedRef<TJsonReader<> > Reader = TJsonReaderFactory<>::Create(InJsonStr);

	TSharedPtr<FJsonObject> JsonObject;
	const bool bDeserializeOk = FJsonSerializer::Deserialize(Reader, JsonObject);

	if (!bDeserializeOk)
	{
		UE_LOG(LogJwt, VeryVerbose, TEXT("deserialization of JWT failed"));
	}
	else if (!JsonObject.IsValid())
	{
		UE_LOG(LogJwt, VeryVerbose, TEXT("no custom properties in JWT"));
	}
	else
	{
		DumpJsonObject(*JsonObject.Get());
	}

	return JsonObject;
}

TSharedPtr<FJsonObject> FJsonWebToken::ParseEncodedJson(const FStringView InEncodedJson)
{
	TSharedPtr<FJsonObject> ParsedObj;

	FString DecodedJson;
	if (Base64Decode(InEncodedJson, DecodedJson))
	{
		ParsedObj = FromJson(DecodedJson);
	}

	return ParsedObj;
}

bool FJsonWebToken::FromString(const FStringView InEncodedJsonWebToken, FJsonWebToken& OutJsonWebToken, const bool bIsSignatureEncoded)
{
	TOptional<FJsonWebToken> JsonWebToken = FromString(InEncodedJsonWebToken, bIsSignatureEncoded);
	if (!JsonWebToken.IsSet())
	{
		return false;
	}

	OutJsonWebToken = MoveTemp(JsonWebToken.GetValue());
	return true;
}

TOptional<FJsonWebToken> FJsonWebToken::FromString(const FStringView InEncodedJsonWebToken, const bool bIsSignatureEncoded)
{
	// Check for the correct number of dots.
	if (Algo::Count(InEncodedJsonWebToken, TEXT('.')) != 2)
	{
		UE_LOG(LogJwt, Verbose, TEXT("[FJsonWebToken::FromString] Failed. Token is not in {header}.{payload}.[{signature}] form."));
		return {};
	}

	// Split out the token string into header, payload, and signature parts.
	FStringView EncodedHeader;
	FStringView EncodedPayload;
	FStringView SignaturePart;
	if (!SplitEncodedJsonWebTokenString(InEncodedJsonWebToken, EncodedHeader, EncodedPayload, SignaturePart))
	{
		return {};
	}

	// Decode and parse the header.
	UE_LOG(LogJwt, VeryVerbose, TEXT("[FJsonWebToken::FromString] Parsing JWT header."));
	const TSharedPtr<FJsonObject> HeaderPtr = ParseEncodedJson(EncodedHeader);
	if (!HeaderPtr)
	{
		UE_LOG(LogJwt, Verbose, TEXT("[FJsonWebToken::FromString] Failed to decode and parse the header."));
		return {};
	}

	// Decode and parse the payload.
	UE_LOG(LogJwt, VeryVerbose, TEXT("[FJsonWebToken::FromString] Parsing JWT payload."));
	const TSharedPtr<FJsonObject> PayloadPtr = ParseEncodedJson(EncodedPayload);
	if (!PayloadPtr)
	{
		UE_LOG(LogJwt, Verbose, TEXT("[FJsonWebToken::FromString] Failed to decode and parse the payload."));
		return {};
	}

	// Decode (but do not parse) the signature if it not empty.
	TOptional<TArray<uint8>> Signature;
	if (!SignaturePart.IsEmpty())
	{
		UE_LOG(LogJwt, VeryVerbose, TEXT("[FJsonWebToken::FromString] Decoding JWT signature."));
		TArray<uint8> SignatureBytes;
		if (!StringViewToBytes(SignaturePart, SignatureBytes, bIsSignatureEncoded))
		{
			UE_LOG(LogJwt, Verbose, TEXT("[FJsonWebToken::FromString] Failed to decode the signature."));
			return {};
		}

		Signature = MoveTemp(SignatureBytes);
	}

	const FJsonWebToken JsonWebToken(InEncodedJsonWebToken, HeaderPtr.ToSharedRef(), PayloadPtr.ToSharedRef(), Signature);

	// Validate the type.  If it exists but is not a JWT, then fail.
	FString Type;
	if (JsonWebToken.GetType(Type) && (Type != TYPE_VALUE_JWT))
	{
		UE_LOG(LogJwt, Verbose, TEXT("[FJsonWebToken::FromString] Failed. Type field exists but is not JWT: \"%s\""), *Type);
		return {};
	}

	return JsonWebToken;
}

FJsonWebToken::FJsonWebToken(
	const FStringView InEncodedJsonWebToken,
	const TSharedRef<FJsonObject>& InHeaderPtr,
	const TSharedRef<FJsonObject>& InPayloadPtr,
	const TOptional<TArray<uint8>>& InSignature
)
	: EncodedJsonWebToken(InEncodedJsonWebToken)
	, Header(InHeaderPtr)
	, Payload(InPayloadPtr)
	, Signature(InSignature)
{
}

bool FJsonWebToken::GetType(FString& OutValue) const
{
	return Header->TryGetStringField(HEADER_TYPE, OutValue);
}

bool FJsonWebToken::GetKeyId(FString& OutValue) const
{
	return Header->TryGetStringField(HEADER_KEY_ID, OutValue);
}

bool FJsonWebToken::GetAlgorithm(FString& OutValue) const
{
	return Header->TryGetStringField(HEADER_ALGORITHM, OutValue);
}

bool FJsonWebToken::GetClaim(const FStringView InName, TSharedPtr<FJsonValue>& OutClaim) const
{
	OutClaim = Payload->TryGetField(FString(InName));
	return OutClaim.IsValid();
}


bool FJsonWebToken::Verify() const
{
	// Signature verification is yet to be implemented, so return true for now.
	// TODO: Add support for RSA crypto algorithm (PlatformCrypto plugin) and key management so that signature verification can be performed.
	return true;
}
