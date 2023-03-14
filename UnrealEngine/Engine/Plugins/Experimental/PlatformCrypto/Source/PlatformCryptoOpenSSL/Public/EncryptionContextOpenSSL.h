// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "PlatformCryptoTypes.h"

#include "Templates/PimplPtr.h"

/** Implementation details for SHA256 computation using OpenSSL */
struct PLATFORMCRYPTOOPENSSL_API FSHA256HasherOpenSSL final
{
	FSHA256HasherOpenSSL(FSHA256HasherOpenSSL&&) = default;
	FSHA256HasherOpenSSL& operator=(FSHA256HasherOpenSSL&&) = default;

	/**
	 * Initialize the necessary state to begin computing a message digest.
	 * It is only necessary to call this function if the object is being used to compute multiple hashes, as the constructor will automatically call it the first time.
	 */
	EPlatformCryptoResult Init();

	/**
	 * Update the message digest computation with additional bytes
	 *
	 * @param InDataBuffer Buffer pointing to the data
	 */
	EPlatformCryptoResult Update(const TArrayView<const uint8> InDataBuffer);

	/**
	 * Finalize the computation of the message digest. After calling this method, Init must be called again if this object is meant to be reused for hashing a new input.
	 *
	 * @param OutDataBuffer A buffer that can hold the message digest bytes. Call GetOutputByteLength to determine the necessary size.
	 */
	EPlatformCryptoResult Finalize(const TArrayView<uint8> OutDataBuffer);

	/**
	 * The final message digest length in bytes.
	 */
	static constexpr const uint32 OutputByteLength = 32;

private:
	FSHA256HasherOpenSSL();
	FSHA256HasherOpenSSL(const FSHA256HasherOpenSSL&) = delete;
	FSHA256HasherOpenSSL& operator=(const FSHA256HasherOpenSSL&) = delete;
	friend class FEncryptionContextOpenSSL;

	struct FImplDetails;
	TPimplPtr<FImplDetails> Inner;
};
using FSHA256Hasher = FSHA256HasherOpenSSL;

/**
 * Interface to certain cryptographic algorithms, using OpenSSL to implement them.
 */
class PLATFORMCRYPTOOPENSSL_API FEncryptionContextOpenSSL
{

public:

	TArray<uint8> Encrypt_AES_256_ECB(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);
	TArray<uint8> Encrypt_AES_256_CBC(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, const TArrayView<const uint8> InitializationVector, EPlatformCryptoResult& OutResult);
	TArray<uint8> Encrypt_AES_256_GCM(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, const TArrayView<const uint8> Nonce, TArray<uint8>& OutAuthTag, EPlatformCryptoResult& OutResult);

	TArray<uint8> Decrypt_AES_256_ECB(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);
	TArray<uint8> Decrypt_AES_256_CBC(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, const TArrayView<const uint8> InitializationVector, EPlatformCryptoResult& OutResult);
	TArray<uint8> Decrypt_AES_256_GCM(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, const TArrayView<const uint8> Nonce, const TArrayView<const uint8> AuthTag, EPlatformCryptoResult& OutResult);

	TUniquePtr<IPlatformCryptoEncryptor> CreateEncryptor_AES_256_ECB(const TArrayView<const uint8> Key);
	TUniquePtr<IPlatformCryptoEncryptor> CreateEncryptor_AES_256_CBC(const TArrayView<const uint8> Key, const TArrayView<const uint8> InitializationVector);
	TUniquePtr<IPlatformCryptoEncryptor> CreateEncryptor_AES_256_GCM(const TArrayView<const uint8> Key, const TArrayView<const uint8> Nonce);

	TUniquePtr<IPlatformCryptoDecryptor> CreateDecryptor_AES_256_ECB(const TArrayView<const uint8> Key);
	TUniquePtr<IPlatformCryptoDecryptor> CreateDecryptor_AES_256_CBC(const TArrayView<const uint8> Key, const TArrayView<const uint8> InitializationVector);
	TUniquePtr<IPlatformCryptoDecryptor> CreateDecryptor_AES_256_GCM(const TArrayView<const uint8> Key, const TArrayView<const uint8> Nonce, const TArrayView<const uint8> AuthTag);

	bool DigestSign_RS256(const TArrayView<const uint8> Message, TArray<uint8>& Signature, FRSAKeyHandle Key);

	bool DigestVerify_PS256(const TArrayView<const char> Message, const TArrayView<const uint8> Signature, const TArrayView<const uint8> PKCS1Key);
	bool DigestVerify_RS256(const TArrayView<const uint8> Message, const TArrayView<const uint8> Signature, FRSAKeyHandle Key);

	bool GenerateKey_RSA(const int32 InNumKeyBits, TArray<uint8>& OutPublicExponent, TArray<uint8>& OutPrivateExponent, TArray<uint8>& OutModulus);

	FRSAKeyHandle CreateKey_RSA(const TArrayView<const uint8> PublicExponent, const TArrayView<const uint8> PrivateExponent, const TArrayView<const uint8> Modulus);
	void DestroyKey_RSA(FRSAKeyHandle Key);
	int32 GetKeySize_RSA(FRSAKeyHandle Key);
	int32 GetMaxDataSize_RSA(FRSAKeyHandle Key);
	int32 EncryptPublic_RSA(TArrayView<const uint8> Source, TArray<uint8>& Dest, FRSAKeyHandle Key);
	int32 EncryptPrivate_RSA(TArrayView<const uint8> Source, TArray<uint8>& Dest, FRSAKeyHandle Key);
	int32 DecryptPublic_RSA(TArrayView<const uint8> Source, TArray<uint8>& Dest, FRSAKeyHandle Key);
	int32 DecryptPrivate_RSA(TArrayView<const uint8> Source, TArray<uint8>& Dest, FRSAKeyHandle Key);

	EPlatformCryptoResult CreateRandomBytes(const TArrayView<uint8> OutData);
	EPlatformCryptoResult CreatePseudoRandomBytes(const TArrayView<uint8> OutData);

	FSHA256Hasher CreateSHA256Hasher();
};

typedef FEncryptionContextOpenSSL FEncryptionContext;
