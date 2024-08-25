// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

struct FSystemFontsRetrieveParams;

//todo: yet to be implemented
namespace UE::Ava::Private::Fonts
{
	void GetSystemFontInfo(TMap<FString, FSystemFontsRetrieveParams>& OutFontsInfo);

	void ListAvailableFontFiles();
}
