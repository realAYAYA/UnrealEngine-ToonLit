// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Containers/UnrealString.h"

namespace UE::Zen
{

struct FZenVersion
{
	uint32 MajorVersion = 0;
	uint32 MinorVersion = 0;
	uint32 PatchVersion = 0;
	FString Details;

	void Reset();
	bool TryParse(const TCHAR* InString);
	FString ToString(bool bDetailed = true) const;
	bool operator<(FZenVersion& Other) const;
};

} // namespace UE::Zen
