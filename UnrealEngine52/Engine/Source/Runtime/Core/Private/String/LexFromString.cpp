// Copyright Epic Games, Inc. All Rights Reserved.

#include "String/LexFromString.h"

#include "Containers/UnrealString.h"
#include "Misc/StringBuilder.h"

void LexFromString(int8& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(int16& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(int32& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(int64& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(uint8& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(uint16& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(uint32& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(uint64& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(float& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(double& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}

void LexFromString(bool& OutValue, const FStringView& InString)
{
	TStringBuilder<64> Builder;
	LexFromString(OutValue, (Builder << InString).ToString());
}
