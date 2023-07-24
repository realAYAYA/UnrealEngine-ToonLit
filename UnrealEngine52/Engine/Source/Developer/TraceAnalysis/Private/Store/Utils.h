// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringView.h"

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
template <typename T>
inline uint32 QuickStoreHash(const T* String)
{
	uint32 Value = 5381;
	for (; *String; ++String)
	{
		Value = ((Value << 5) + Value) + *String;
	}
	return Value;
}

////////////////////////////////////////////////////////////////////////////////
template <typename CharType>
inline uint32 QuickStoreHash(TStringView<CharType> View)
{
	const CharType* String = View.GetData();
	uint32 Value = 5381;
	for (int i = View.Len(); i > 0; --i, ++String)
	{
		Value = ((Value << 5) + Value) + *String;
	}
	return Value;
}

} // namespace Trace
} // namespace UE
