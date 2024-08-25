// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

namespace uba
{
	class Logger;

	enum CryptoKey : u64 {};
	inline constexpr CryptoKey InvalidCryptoKey = (CryptoKey)0;

	class Crypto
	{
	public:
		static CryptoKey CreateKey(Logger& logger, const u8* key128);
		static CryptoKey DuplicateKey(Logger& logger, CryptoKey original);
		static void DestroyKey(CryptoKey key);

		static bool Encrypt(Logger& logger, CryptoKey key, u8* data, u32 size);
		static bool Decrypt(Logger& logger, CryptoKey key, u8* data, u32 size);
	};


}
