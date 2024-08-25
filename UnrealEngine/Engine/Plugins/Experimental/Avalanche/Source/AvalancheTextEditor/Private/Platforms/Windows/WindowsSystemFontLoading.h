// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"

struct FSystemFontsRetrieveParams;

namespace UE::Ava::Private::Fonts
{
	void GetSystemFontInfo(TMap<FString, FSystemFontsRetrieveParams>& OutFontsInfo);

	void ListAvailableFontFiles();
}
