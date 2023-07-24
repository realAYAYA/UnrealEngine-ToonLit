// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareSDKObject.h"

#include "Misc/TextureShareCoreStrings.h"
#include "Misc/TextureShareStrings.h"
#include "Misc/TextureShareDisplayClusterStrings.h"

/**
 * TextureShare object descriptor
 */
struct FTextureShareObjectDesc
{
	FTextureShareObjectDesc(ETextureShareDeviceType InDeviceType, const wchar_t* InShareName = nullptr, const wchar_t* InProcessName = nullptr, int32 InMinConnectionsCnt = 0)
		: DeviceType(InDeviceType)
		, ShareName(GetShareName(InShareName))
		, ProcessName(GetProcessName(InProcessName))
		, MinConnectionsCnt(InMinConnectionsCnt)
	{ }

private:
	static inline const wchar_t* GetShareName(const wchar_t* InShareName)
	{
		if (InShareName==nullptr || *InShareName == L'\0')
		{
			// Use default share name
			return UE::TextureShareCoreStrings::DefaultShareName;
		}

		return InShareName;
	}

	static inline const wchar_t* GetProcessName(const wchar_t* InProcessName)
	{
		if (InProcessName == nullptr || *InProcessName == L'\0')
		{
			return UE::TextureShareCoreStrings::DefaultProcessName::SDK;
		}

		return InProcessName;
	}

public:
	// Render device type
	const ETextureShareDeviceType DeviceType = ETextureShareDeviceType::Undefined;

	// Unique share name. Objects with the same name will interact in IPC
	const FString ShareName = UE::TextureShareCoreStrings::DefaultShareName;

	// Process name
	const FString ProcessName = UE::TextureShareCoreStrings::DefaultProcessName::SDK;

	// Minimum number of connected processes required to start a connection
	const int32 MinConnectionsCnt = 0;
};
