// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCacheMethod.h"

#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "String/ParseTokens.h"

namespace UE::DerivedData::Private
{

template <typename CharType>
static TStringBuilderBase<CharType>& MethodToString(TStringBuilderBase<CharType>& Builder, const ECacheMethod Method)
{
	switch (Method)
	{
	case ECacheMethod::Put:       return Builder << ANSITEXTVIEW("Put");
	case ECacheMethod::Get:       return Builder << ANSITEXTVIEW("Get");
	case ECacheMethod::PutValue:  return Builder << ANSITEXTVIEW("PutValue");
	case ECacheMethod::GetValue:  return Builder << ANSITEXTVIEW("GetValue");
	case ECacheMethod::GetChunks: return Builder << ANSITEXTVIEW("GetChunks");
	}
	return Builder << ANSITEXTVIEW("Unknown");
}

template <typename CharType>
static bool MethodFromString(ECacheMethod& OutMethod, const TStringView<CharType> String)
{
	const auto ConvertedString = StringCast<UTF8CHAR, 16>(String.GetData(), String.Len());
	if (ConvertedString == UTF8TEXTVIEW("Put"))
	{
		OutMethod = ECacheMethod::Put;
	}
	else if (ConvertedString == UTF8TEXTVIEW("Get"))
	{
		OutMethod = ECacheMethod::Get;
	}
	else if (ConvertedString == UTF8TEXTVIEW("PutValue"))
	{
		OutMethod = ECacheMethod::PutValue;
	}
	else if (ConvertedString == UTF8TEXTVIEW("GetValue"))
	{
		OutMethod = ECacheMethod::GetValue;
	}
	else if (ConvertedString == UTF8TEXTVIEW("GetChunks"))
	{
		OutMethod = ECacheMethod::GetChunks;
	}
	else
	{
		return false;
	}
	return true;
}

} // UE::DerivedData::Private

namespace UE::DerivedData
{

FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, ECacheMethod Method) { return Private::MethodToString(Builder, Method); }
FWideStringBuilderBase& operator<<(FWideStringBuilderBase& Builder, ECacheMethod Method) { return Private::MethodToString(Builder, Method); }
FUtf8StringBuilderBase& operator<<(FUtf8StringBuilderBase& Builder, ECacheMethod Method) { return Private::MethodToString(Builder, Method); }

bool TryLexFromString(ECacheMethod& OutMethod, FUtf8StringView String) { return Private::MethodFromString(OutMethod, String); }
bool TryLexFromString(ECacheMethod& OutMethod, FWideStringView String) { return Private::MethodFromString(OutMethod, String); }

FCbWriter& operator<<(FCbWriter& Writer, const ECacheMethod Method)
{
	Writer.AddString(WriteToUtf8String<16>(Method));
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, ECacheMethod& OutMethod)
{
	if (TryLexFromString(OutMethod, Field.AsString()))
	{
		return true;
	}
	OutMethod = {};
	return false;
}

FCacheMethodFilter FCacheMethodFilter::Parse(const FStringView MethodNames)
{
	FCacheMethodFilter MethodFilter;
	MethodFilter.MethodMask = ~uint32(0);
	String::ParseTokensMultiple(MethodNames, {TEXT('+'), TEXT(',')}, [&MethodFilter](FStringView MethodName)
	{
		ECacheMethod Method;
		if (TryLexFromString(Method, MethodName))
		{
			MethodFilter.MethodMask &= ~(1 << uint32(Method));
		}
	});
	return MethodFilter;
}

} // UE::DerivedData
