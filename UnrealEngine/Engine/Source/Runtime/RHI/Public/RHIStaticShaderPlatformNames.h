// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIShaderPlatform.h"
#include "UObject/NameTypes.h"

class FStaticShaderPlatformNames
{
private:
	static const uint32 NumPlatforms = DDPI_NUM_STATIC_SHADER_PLATFORMS;

	struct FPlatform
	{
		FName Name;
		FName ShaderPlatform;
		FName ShaderFormat;
	} Platforms[NumPlatforms];

	FStaticShaderPlatformNames()
	{
#ifdef DDPI_SHADER_PLATFORM_NAME_MAP
		struct FStaticNameMapEntry
		{
			FName Name;
			FName PlatformName;
			int32 Index;
		} NameMap[] =
		{
			DDPI_SHADER_PLATFORM_NAME_MAP
		};

		for (int32 MapIndex = 0; MapIndex < UE_ARRAY_COUNT(NameMap); ++MapIndex)
		{
			FStaticNameMapEntry const& Entry = NameMap[MapIndex];
			check(IsStaticPlatform(EShaderPlatform(Entry.Index)));
			uint32 PlatformIndex = Entry.Index - SP_StaticPlatform_First;

			FPlatform& Platform = Platforms[PlatformIndex];
			check(Platform.Name == NAME_None); // Check we've not already seen this platform

			Platform.Name = Entry.PlatformName;
			Platform.ShaderPlatform = FName(*FString::Printf(TEXT("SP_%s"), *Entry.Name.ToString()), FNAME_Add);
			Platform.ShaderFormat = FName(*FString::Printf(TEXT("SF_%s"), *Entry.Name.ToString()), FNAME_Add);
		}
#endif
	}

public:
	static inline FStaticShaderPlatformNames const& Get()
	{
		static FStaticShaderPlatformNames Names;
		return Names;
	}

	static inline bool IsStaticPlatform(EShaderPlatform Platform)
	{
		return Platform >= SP_StaticPlatform_First && Platform <= SP_StaticPlatform_Last;
	}

	inline const FName& GetShaderPlatform(EShaderPlatform Platform) const
	{
		return Platforms[GetStaticPlatformIndex(Platform)].ShaderPlatform;
	}

	inline const FName& GetShaderFormat(EShaderPlatform Platform) const
	{
		return Platforms[GetStaticPlatformIndex(Platform)].ShaderFormat;
	}

	inline const FName& GetPlatformName(EShaderPlatform Platform) const
	{
		return Platforms[GetStaticPlatformIndex(Platform)].Name;
	}

private:
	static inline uint32 GetStaticPlatformIndex(EShaderPlatform Platform)
	{
		check(IsStaticPlatform(Platform));
		return uint32(Platform) - SP_StaticPlatform_First;
	}
};
