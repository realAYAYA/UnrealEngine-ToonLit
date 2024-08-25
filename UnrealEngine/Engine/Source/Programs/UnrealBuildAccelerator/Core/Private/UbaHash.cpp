// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaHash.h"
#include <blake3.h>

namespace uba
{
	StringKeyHasher::StringKeyHasher()
	{
		static_assert(sizeof(hasher) == sizeof(blake3_hasher));
		blake3_hasher_init((blake3_hasher*)&hasher);
	}

	void StringKeyHasher::Update(const tchar* str, u64 strLen)
	{
		CHECK_PATH(str);
		if (strLen != 0)
			blake3_hasher_update((blake3_hasher*)&hasher, str, strLen * sizeof(tchar));
	}

	StringKey ToStringKey(const tchar* str, u64 strLen)
	{
		CHECK_PATH(str);
		blake3_hasher hasher;
		blake3_hasher_init(&hasher);
		blake3_hasher_update(&hasher, str, strLen * sizeof(tchar));
		uint8_t output[BLAKE3_OUT_LEN];
		blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
		return (StringKey&)output;
	}

	StringKey ToStringKey(const StringBufferBase& b)
	{
		return ToStringKey(b.data, b.count);
	}

	StringKey ToStringKeyLower(const StringBufferBase& b)
	{
		StringBuffer<> temp;
		temp.Append(b).MakeLower();
		return ToStringKey(temp.data, temp.count);
	}
	StringKey ToStringKey(const StringKeyHasher& hasher, const tchar* str, u64 strLen)
	{
		CHECK_PATH(str);
		StringKeyHasher temp(hasher);
		blake3_hasher_update((blake3_hasher*)&temp.hasher, str, strLen * sizeof(tchar));
		uint8_t output[BLAKE3_OUT_LEN];
		blake3_hasher_finalize((blake3_hasher*)&temp.hasher, output, BLAKE3_OUT_LEN);
		return (StringKey&)output;
	}

	StringKey ToStringKey(const StringKeyHasher& hasher)
	{
		StringKeyHasher temp(hasher);
		uint8_t output[BLAKE3_OUT_LEN];
		blake3_hasher_finalize((blake3_hasher*)&temp.hasher, output, BLAKE3_OUT_LEN);
		return (StringKey&)output;
	}

	StringKey ToStringKeyNoCheck(const tchar* str, u64 strLen)
	{
		blake3_hasher hasher;
		blake3_hasher_init(&hasher);
		blake3_hasher_update(&hasher, str, strLen * sizeof(tchar));
		uint8_t output[BLAKE3_OUT_LEN];
		blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
		return (StringKey&)output;
	}

	CasKeyHasher::CasKeyHasher()
	{
		static_assert(sizeof(hasher) == sizeof(blake3_hasher));
		blake3_hasher_init((blake3_hasher*)&hasher);
	}

	void CasKeyHasher::Update(const void* data, u64 bytes)
	{
		blake3_hasher_update((blake3_hasher*)&hasher, data, bytes);
	}

	CasKey ToCasKey(const CasKeyHasher& hasher, bool compressed)
	{
		CasKeyHasher temp(hasher);
		uint8_t output[BLAKE3_OUT_LEN];
		blake3_hasher_finalize((blake3_hasher*)&temp.hasher, output, BLAKE3_OUT_LEN);
		return AsCompressed((CasKey&)output, compressed);
	}
}
