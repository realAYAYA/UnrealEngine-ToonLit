// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "UObject/NameTypes.h"

class FString;
class FConfigCacheIni;
class USoundWave;

namespace Audio
{
	class FAudioFormatSettings
	{
	public:
		ENGINE_API FAudioFormatSettings(FConfigCacheIni* InConfigSystem, const FString& InConfigFilename, const FString& InPlatformIdentifierForLogging);
		~FAudioFormatSettings() = default;

		ENGINE_API FName GetWaveFormat(const USoundWave* Wave) const;
		ENGINE_API void GetAllWaveFormats(TArray<FName>& OutFormats) const;
		ENGINE_API void GetWaveFormatModuleHints(TArray<FName>& OutHints) const;

		FName GetFallbackFormat() const { return FallbackFormat; }

	private:
		ENGINE_API void ReadConfiguration(FConfigCacheIni*, const FString& InConfigFilename, const FString& InPlatformIdentifierForLogging);

		TArray<FName> AllWaveFormats;
		TArray<FName> WaveFormatModuleHints;
		FName PlatformFormat;
		FName PlatformStreamingFormat;
		FName FallbackFormat;
	};
}
