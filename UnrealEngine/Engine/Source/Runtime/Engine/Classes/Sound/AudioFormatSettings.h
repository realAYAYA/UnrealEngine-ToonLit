// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "UObject/NameTypes.h"

class FString;
class FConfigCacheIni;
class USoundWave;

namespace Audio
{
	class ENGINE_API FAudioFormatSettings
	{
	public:
		FAudioFormatSettings(FConfigCacheIni* InConfigSystem, const FString& InConfigFilename, const FString& InPlatformIdentifierForLogging);
		~FAudioFormatSettings() = default;

		FName GetWaveFormat(const USoundWave* Wave) const;
		void GetAllWaveFormats(TArray<FName>& OutFormats) const;
		void GetWaveFormatModuleHints(TArray<FName>& OutHints) const;

		FName GetFallbackFormat() const { return FallbackFormat; }

	private:
		void ReadConfiguration(FConfigCacheIni*, const FString& InConfigFilename, const FString& InPlatformIdentifierForLogging);

		TArray<FName> AllWaveFormats;
		TArray<FName> WaveFormatModuleHints;
		FName PlatformFormat;
		FName PlatformStreamingFormat;
		FName FallbackFormat;
	};
}
