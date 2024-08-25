// Copyright Epic Games, Inc. All Rights Reserved.

#include "VerseScope.h"

TOptional<EVerseScope::Type> EVerseScope::FromString(const TCHAR* Text)
{
	if (!FCString::Stricmp(Text, TEXT("PublicAPI")))
	{
		return { EVerseScope::PublicAPI };
	}
	else if (!FCString::Stricmp(Text, TEXT("InternalAPI")))
	{
		return { EVerseScope::InternalAPI };
	}
	else if (!FCString::Stricmp(Text, TEXT("PublicUser")))
	{
		return { EVerseScope::PublicUser };
	}
	else if (!FCString::Stricmp(Text, TEXT("InternalUser")))
	{
		return { EVerseScope::InternalUser };
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
	case PublicAPI:    return TEXT("PublicAPI");
	case InternalAPI:  return TEXT("InternalAPI");
	case PublicUser:   return TEXT("PublicUser");
	case InternalUser: return TEXT("InternalUser");
	default:
		ensure(false);
		return TEXT("<unknown>");
	}
}
