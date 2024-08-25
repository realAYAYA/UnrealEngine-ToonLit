// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DerivedDataPluginInterface.h"

class IAudioFormat;
class USoundWave;
struct FPlatformAudioCookOverrides;
struct FAudioCookInputs;

class FDerivedAudioDataCompressor : public FDerivedDataPluginInterface
{
private:
	TUniquePtr<FAudioCookInputs> CookInputs;

public:
	ENGINE_API FDerivedAudioDataCompressor(USoundWave* InSoundNode, FName InBaseFormat, FName InHashedFormat, const FPlatformAudioCookOverrides* InCompressionOverrides, const ITargetPlatform* InTargetPlatform=nullptr);

	virtual const TCHAR* GetPluginName() const override
	{
		return TEXT("Audio");
	}

	virtual const TCHAR* GetVersionString() const override;

	virtual FString GetPluginSpecificCacheKeySuffix() const override;
	
	virtual bool IsBuildThreadsafe() const override;

	virtual bool Build(TArray<uint8>& OutData) override;
};
