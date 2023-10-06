// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringView.h"
#include "CoreTypes.h"
#include "DerivedDataSharedStringFwd.h"
#include "HAL/MemoryBase.h"
#include "HAL/UnrealMemory.h"
#include "Misc/CString.h"

#include <atomic>

#define UE_API DERIVEDDATACACHE_API

class FCbFieldView;
class FCbWriter;

namespace UE::DerivedData
{

/**
 * A reference to an immutable, shared, reference-counted string.
 *
 * Prefer TStringView when there is a clear single point of ownership with a longer lifetime than
 * the references to the string. This type is meant for cases where a shared string does not have
 * an obvious owner or where the lifetime is not easy to manage.
 *
 * The string is stored as a pointer to the start of the null-terminated string which is preceded
 * by a 4-byte reference count and a 4-byte size.
 */
template <typename CharType>
class TSharedString
{
public:
	TSharedString() = default;
	TSharedString(TSharedString&& String);
	TSharedString(const TSharedString& String);
	TSharedString& operator=(TSharedString&& String);
	TSharedString& operator=(const TSharedString& String);

	TSharedString(TStringView<CharType> String);
	TSharedString& operator=(TStringView<CharType> String);

	~TSharedString() { Reset(); }

	inline void Reset() { Release(Chars); Chars = nullptr; }

	[[nodiscard]] inline bool IsEmpty() const { return !Chars; }
	[[nodiscard]] inline int32 Len() const { return Chars ? reinterpret_cast<const int32*>(Chars)[-1] : 0; }
	[[nodiscard]] inline const CharType* operator*() const { return Chars ? Chars : &NullChar; }

	static const TSharedString Empty;

private:
	static inline void AddRef(CharType* Chars);
	static inline void Release(CharType* Chars);

	CharType* Chars = nullptr;

	static constexpr inline CharType NullChar{};
};

template <typename CharType>
inline const TSharedString<CharType> TSharedString<CharType>::Empty;

template <typename CharType>
constexpr inline const CharType* GetData(const TSharedString<CharType> String)
{
	return *String;
}

template <typename CharType>
constexpr inline auto GetNum(const TSharedString<CharType> String)
{
	return String.Len();
}

template <typename CharType>
inline TSharedString<CharType>::TSharedString(TSharedString&& String)
	: Chars(String.Chars)
{
	String.Chars = nullptr;
}

template <typename CharType>
TSharedString<CharType>::TSharedString(const TSharedString& String)
	: Chars(String.Chars)
{
	AddRef(Chars);
}

template <typename CharType>
TSharedString<CharType>& TSharedString<CharType>::operator=(TSharedString&& String)
{
	if (this != &String)
	{
		Release(Chars);
		Chars = String.Chars;
		String.Chars = nullptr;
	}
	return *this;
}

template <typename CharType>
TSharedString<CharType>& TSharedString<CharType>::operator=(const TSharedString& String)
{
	CharType* OldChars = Chars;
	CharType* NewChars = String.Chars;
	AddRef(NewChars);
	Release(OldChars);
	Chars = NewChars;
	return *this;
}

template <typename CharType>
TSharedString<CharType>::TSharedString(const TStringView<CharType> String)
{
	if (const int32 Length = String.Len())
	{
		static_assert(alignof(int32) <= MIN_ALIGNMENT);
		static_assert(alignof(int32) >= alignof(CharType));
		static_assert(sizeof(int32) == sizeof(std::atomic<int32>));
		const SIZE_T Size = sizeof(int32) + sizeof(int32) + sizeof(CharType) + sizeof(CharType) * Length;
		int32* const Header = static_cast<int32*>(FMemory::Malloc(Size));
		new(&Header[0]) std::atomic<int32>(1); // Ref Count
		new(&Header[1]) int32(Length);
		Chars = reinterpret_cast<CharType*>(&Header[2]);
		String.CopyString(Chars, Length);
		Chars[Length] = CharType(0);
	}
}

template <typename CharType>
TSharedString<CharType>& TSharedString<CharType>::operator=(const TStringView<CharType> String)
{
	return *this = TSharedString(String);
}

template <typename CharType>
void TSharedString<CharType>::AddRef(CharType* const MaybeNullChars)
{
	if (MaybeNullChars)
	{
		static_assert(sizeof(int32) == sizeof(std::atomic<int32>));
		int32* const Header = reinterpret_cast<int32*>(MaybeNullChars) - 2;
		std::atomic<int32>& RefCount = reinterpret_cast<std::atomic<int32>&>(Header[0]);
		RefCount.fetch_add(1, std::memory_order_acquire);
	}
}

template <typename CharType>
void TSharedString<CharType>::Release(CharType* const MaybeNullChars)
{
	if (MaybeNullChars)
	{
		static_assert(sizeof(int32) == sizeof(std::atomic<int32>));
		int32* const Header = reinterpret_cast<int32*>(MaybeNullChars) - 2;
		std::atomic<int32>& RefCount = reinterpret_cast<std::atomic<int32>&>(Header[0]);
		if (RefCount.fetch_sub(1, std::memory_order_release) == 1)
		{
			FMemory::Free(Header);
		}
	}
}

template <typename CharType>
inline uint32 GetTypeHash(const TSharedString<CharType>& String)
{
	return GetTypeHash(MakeStringView(String));
}

template <typename CharType>
inline bool operator==(const TSharedString<CharType>& Lhs, const TSharedString<CharType>& Rhs)
{
	return MakeStringView(Lhs).Equals(MakeStringView(Rhs), ESearchCase::IgnoreCase);
}

template <typename CharType>
inline bool operator<(const TSharedString<CharType>& Lhs, const TSharedString<CharType>& Rhs)
{
	return MakeStringView(Lhs).Compare(MakeStringView(Rhs), ESearchCase::IgnoreCase) < 0;
}

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FUtf8SharedString& OutString);
UE_API bool LoadFromCompactBinary(FCbFieldView Field, FWideSharedString& OutString);

} // UE::DerivedData

#undef UE_API
