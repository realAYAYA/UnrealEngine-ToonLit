// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JwtGlobals.h"
#include "Dom/JsonObject.h"

class JWT_API FJsonWebToken
{
public:
	/**
	 * Creates a JWT from the provided string.
	 * The string must consist of 3 base64 url encoded parts: a header, payload, and signature.
	 * The parts must be split by a period character.
	 * The signature part is optional. If the signature is excluded, the string must still contain a period character in its place.
	 * Valid formats: "header.payload.signature" and "header.payload."
	 *
	 * @param InJsonWebTokenString The string to decode.
	 * @return An optional set with a FJsonWebToken if the JWT was successfully decoded.
	 */
	static TOptional<FJsonWebToken> FromString(const FStringView InEncodedJsonWebToken, const bool bIsSignatureEncoded);

	static bool FromString(const FStringView InEncodedJsonWebToken, FJsonWebToken& OutJsonWebToken, const bool bIsSignatureEncoded);

	/**
	 * Gets the type.
	 *
	 * @param OutValue The value to output on success.
	 * @return Whether the value exists and was successfully outputted.
	 */
	bool GetType(FString& OutValue) const;

	/**
	 * Gets the key id.
	 *
	 * @param OutValue The value to output on success.
	 * @return Whether the value exists and was successfully outputted.
	 */
	bool GetKeyId(FString& OutValue) const;

	/**
	 * Gets the algorithm that was used to construct the signature.
	 *
	 * @param OutValue The value to output on success.
	 * @return Whether the value exists and was successfully outputted.
	 */
	bool GetAlgorithm(FString& OutValue) const;

	/**
	 * Gets a claim by name.
	 * This method can be used to get custom claims that are not reserved as part of the JWT specification.
	 *
	 * @param InName The name of the claim.
	 * @param OutClaim The json value to output on success.
	 * @return Whether the claim exists and was successfully outputted.
	 */
	bool GetClaim(const FStringView InName, TSharedPtr<FJsonValue>& OutClaim) const;

	/**
	 * Gets a claim by name.
	 * This method can be used to get custom claims that are not reserved as part of the JWT specification.
	 *
	 * @param InName The name of the claim.
	 * @return The json value to output on success, or an invalid shared pointer on failure.
	 */
	template<EJson JsonType>
	TSharedPtr<FJsonValue> GetClaim(const FStringView InName) const
	{
		return Payload->GetField<JsonType>(FString(InName));
	}

	/**
	 * Perform verification of the encoded header and encoded payload.
	 * The is done using the cryptographic algorithm specified in the header, and a secret value obtained
	 * by key-lookup, to generate a signature.  The generated signature must be identical to the signature
	 * provided in the JWT for verification to succeed.
	 * 
	 * @return true if the header and payload were verified successfully (signature match), otherwise false.
	 */
	bool Verify() const;

private:
	FJsonWebToken(const FStringView InEncodedJsonWebToken, const TSharedRef<FJsonObject>& InHeaderPtr,
		const TSharedRef<FJsonObject>& InPayloadPtr, const TOptional<TArray<uint8>>& InSignature);

	static void DumpJsonObject(const FJsonObject& InJsonObject);

	static TSharedPtr<FJsonObject> FromJson(const FString& InJsonStr);

	static TSharedPtr<FJsonObject> ParseEncodedJson(const FStringView InEncodedJson);

public:
	// JWT payload registered claim field names
	static const TCHAR *const CLAIM_ISSUER;
	static const TCHAR *const CLAIM_ISSUED_AT;
	static const TCHAR *const CLAIM_EXPIRATION;
	static const TCHAR *const CLAIM_SUBJECT;
	static const TCHAR *const CLAIM_AUDIENCE;

	// JWT header field names
	static const TCHAR* const HEADER_TYPE;
	static const TCHAR* const HEADER_KEY_ID;
	static const TCHAR* const HEADER_ALGORITHM;

	// JWT header expected values
	static const TCHAR* const TYPE_VALUE_JWT;

private:
	/** The full encoded JWT. */
	FString EncodedJsonWebToken;

	/** The decoded and parsed header. */
	TSharedRef<FJsonObject> Header;

	/** The decoded and parsed payload. */
	TSharedRef<FJsonObject> Payload;

	/** The decoded signature. */
	TOptional<TArray<uint8>> Signature;
};
