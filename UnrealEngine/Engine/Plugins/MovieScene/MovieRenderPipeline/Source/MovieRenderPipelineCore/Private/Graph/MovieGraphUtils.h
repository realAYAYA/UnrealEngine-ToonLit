// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Forward declares
class FAudioDevice;
namespace Audio
{
	class FMixerDevice;
}

namespace UE::MovieGraph
{
	/**
	 * Generate a unique name given a set of existing names and the desired base name. The base name will
	 * be given a postfix value if it conflicts with an existing name (eg, if the base name is "Foo" but
	 * there's already an existing name "Foo", the generated name would be "Foo 1").
	 */
	FString GetUniqueName(const TArray<FString>& InExistingNames, const FString& InBaseName);

	namespace Audio
	{
		/** Gets the audio device from the supplied world context (or nullptr if it could not be determined). */
		FAudioDevice* GetAudioDeviceFromWorldContext(const UObject* InWorldContextObject);

		/** Gets the audio mixer from the supplied world context (or nullptr if it could not be determined). */
		::Audio::FMixerDevice* GetAudioMixerDeviceFromWorldContext(const UObject* InWorldContextObject);

		/** Determines if the pipeline can generate audio. */
		bool IsMoviePipelineAudioOutputSupported(const UObject* InWorldContextObject);
	}
}
