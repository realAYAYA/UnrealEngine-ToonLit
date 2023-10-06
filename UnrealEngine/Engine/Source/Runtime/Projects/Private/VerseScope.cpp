// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseScope.h"

TOptional<EVerseScope::Type> EVerseScope::FromString(const TCHAR* Text)
{
	if (!FCString::Stricmp(Text, TEXT("InternalAPI")))
	{
		return {EVerseScope::InternalAPI};
	}
	else if (!FCString::Stricmp(Text, TEXT("PublicAPI")))
	{
		return {EVerseScope::PublicAPI};
	}
	else if (!FCString::Stricmp(Text, TEXT("User")))
	{
		return {EVerseScope::User};
	}
	else
	{
		return {};
	}
}

const TCHAR* EVerseScope::ToString(const Type Value)
{
	switch (Value)
	{
	case InternalAPI: return TEXT("InternalAPI");
	case PublicAPI:   return TEXT("PublicAPI");
	case User:        return TEXT("User");
	default:
		ensure(false);
		return TEXT("<unknown>");
	}
}
