// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaCrypto.h"
#include "UbaLogger.h"
#include "UbaPlatform.h"
#include "UbaSynchronization.h"

#if PLATFORM_WINDOWS
#define UBA_CRYPTO_TYPE 1
#else
#define UBA_CRYPTO_TYPE 0 
#endif

#if UBA_CRYPTO_TYPE == 1
	#include <winternl.h>
	#include <bcrypt.h>
	#pragma comment (lib, "bcrypt.lib")
#endif // UBA_CRYPTO_TYPE

namespace uba
{
	inline constexpr u32 kAesBytes128 = 16;

	CryptoKey Crypto::CreateKey(Logger& logger, const u8* key128)
	{
#if UBA_CRYPTO_TYPE == 1
		BCRYPT_ALG_HANDLE providerHandle = NULL;
		NTSTATUS res = BCryptOpenAlgorithmProvider(&providerHandle, BCRYPT_AES_ALGORITHM, NULL, 0);
		if (!NT_SUCCESS(res))
		{
			logger.Error(L"ERROR: BCryptOpenAlgorithmProvider - Failed to open aes algorithm (0x%x)", res);
			return InvalidCryptoKey;
		}
		if (!providerHandle)
		{
			logger.Error(L"ERROR: BCryptOpenAlgorithmProvider - Returned null handle");
			return InvalidCryptoKey;
		}
		auto g = MakeGuard([&]() { BCryptCloseAlgorithmProvider(&providerHandle, 0); });

		//ULONG size = 0;
		//u32 len = 0;
		//NTSTATUS ret = BCryptGetProperty(providerHandle, BCRYPT_OBJECT_LENGTH, (UCHAR*)&len, sizeof(len), &size, 0);
		//UBA_ASSERT(NT_SUCCESS(ret));
		//void* buf = calloc(1, len);
		u32 objectBufferLen = 0;
		u8* objectBuffer = nullptr;


		BCRYPT_KEY_HANDLE keyHandle = NULL;
		res = BCryptGenerateSymmetricKey(providerHandle, &keyHandle, objectBuffer, objectBufferLen, (u8*)key128, kAesBytes128, 0);
		if (!NT_SUCCESS(res))
		{
			logger.Error(L"ERROR: BCryptGenerateSymmetricKey - Failed to generate symmetric key (0x%x)", res);
			return InvalidCryptoKey;
		}

		return (CryptoKey)(u64)keyHandle;
#else
		logger.Error(TC("ERROR: Crypto not supported on non-windows platforms"));
		return InvalidCryptoKey;
#endif // UBA_CRYPTO_TYPE
	}

	CryptoKey Crypto::DuplicateKey(Logger& logger, CryptoKey original)
	{
#if UBA_CRYPTO_TYPE == 1
		BCRYPT_KEY_HANDLE newKey;
		u32 objectBufferLen = 0;
		u8* objectBuffer = nullptr;
		NTSTATUS res = BCryptDuplicateKey((BCRYPT_KEY_HANDLE)original, &newKey, objectBuffer, objectBufferLen, 0);
		if (NT_SUCCESS(res))
			return (CryptoKey)(u64)newKey;
		logger.Error(L"ERROR: BCryptDuplicateKey failed (0x%x)", res);
		return InvalidCryptoKey;
#else
		return InvalidCryptoKey;
#endif // UBA_CRYPTO_TYPE
	}

	void Crypto::DestroyKey(CryptoKey key)
	{
#if UBA_CRYPTO_TYPE == 1
		BCryptDestroyKey((BCRYPT_KEY_HANDLE)key);
#endif // UBA_CRYPTO_TYPE
	}

	bool BCryptEncryptDecrypt(Logger& logger, bool encrypt, CryptoKey key, u8* data, u32 size)
	{
#if UBA_CRYPTO_TYPE == 1
		u8 objectBuffer[1024];
		u32 objectBufferLen = sizeof(objectBuffer);

		u32 alignedSize = (size / kAesBytes128) * kAesBytes128;
		BCRYPT_KEY_HANDLE newKey;
		NTSTATUS res = BCryptDuplicateKey((BCRYPT_KEY_HANDLE)key, &newKey, objectBuffer, objectBufferLen, 0);
		if (!NT_SUCCESS(res))
		{
			logger.Error(L"ERROR: BCryptDuplicateKey failed (0x%x)", res);
			return false;
		}
		auto g = MakeGuard([&]() { BCryptDestroyKey(newKey); });

		u8* initVector = nullptr;
		u32 initVectorSize = 0;
		ULONG cipherTextLength = 0;
		res = encrypt ?
			BCryptEncrypt(newKey, data, alignedSize, NULL, initVector, initVectorSize, data, alignedSize, &cipherTextLength, 0) :
			BCryptDecrypt(newKey, data, alignedSize, NULL, initVector, initVectorSize, data, alignedSize, &cipherTextLength, 0);

		if (!NT_SUCCESS(res))
		{
			logger.Error(L"ERROR: %s failed (0x%x)", (encrypt ? L"BCryptEncrypt" : L"BCryptDecrypt"), res);
			return false;
		}
		if (cipherTextLength != alignedSize)
		{
			logger.Error(L"ERROR: %s cipher text length does not match aligned size", (encrypt ? L"BCryptEncrypt" : L"BCryptDecrypt"));
			return false;
		}
#endif
		return true;
	}

	bool Crypto::Encrypt(Logger& logger, CryptoKey key, u8* data, u32 size)
	{
		return BCryptEncryptDecrypt(logger, true, key, data, size);
	}

	bool Crypto::Decrypt(Logger& logger, CryptoKey key, u8* data, u32 size)
	{
		return BCryptEncryptDecrypt(logger, false, key, data, size);
	}

}