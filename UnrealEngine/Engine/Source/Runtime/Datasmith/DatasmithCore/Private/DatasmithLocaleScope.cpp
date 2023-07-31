// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithLocaleScope.h"

#include "Containers/StringConv.h"

#include <clocale>


FDatasmithLocaleScope::FDatasmithLocaleScope(const TCHAR* InScopedLocale)
{
	if (const char* saved = std::setlocale(LC_ALL, nullptr))
	{
		SavedLocale = ANSI_TO_TCHAR(saved);
	}
	ScopedLocale = ANSI_TO_TCHAR(std::setlocale(LC_ALL, TCHAR_TO_ANSI(InScopedLocale)));
}

FDatasmithLocaleScope::~FDatasmithLocaleScope()
{
	std::setlocale(LC_ALL, TCHAR_TO_ANSI(*SavedLocale));
}

