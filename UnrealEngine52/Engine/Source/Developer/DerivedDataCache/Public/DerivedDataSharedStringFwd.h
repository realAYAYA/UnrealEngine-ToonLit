// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Traits/ElementType.h"
#include "Traits/IsContiguousContainer.h"

namespace UE::DerivedData
{

template <typename CharType> class TSharedString;

using FSharedString = TSharedString<TCHAR>;
using FAnsiSharedString = TSharedString<ANSICHAR>;
using FWideSharedString = TSharedString<WIDECHAR>;
using FUtf8SharedString = TSharedString<UTF8CHAR>;

} // UE::DerivedData

template <typename CharType>
struct TIsContiguousContainer<UE::DerivedData::TSharedString<CharType>>
{
	static constexpr bool Value = true;
};

template <typename CharType>
struct TElementType<UE::DerivedData::TSharedString<CharType>>
{
	using Type = CharType;
};
