// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Interfaces/IBuildManifest.h"

namespace BuildPatchServices
{
	/**
	 * An enum defining the desired policy for requesting an optimised delta.
	 */
	enum class EDeltaPolicy : uint32
	{
		// Try to fetch, but continue without if request fail.
		TryFetchContinueWithout,

		// Expect the delta to exist, hard fail the installation if it could not be retrieved.
		Expect,

		// Expect the delta to not exist, skipping any attempt to use one.
		Skip,
	};

	/**
	 * Returns the string representation of the EDeltaPolicy value. Used for analytics and logging only.
	 * @param DeltaPolicy     The value.
	 * @return the enum's string representation.
	 */
	inline const FString& EnumToString(const EDeltaPolicy& DeltaPolicy)
	{
		static const FString TryFetchContinueWithout(TEXT("EDeltaPolicy::TryFetchContinueWithout"));
		static const FString Expect(TEXT("EDeltaPolicy::Expect"));
		static const FString Skip(TEXT("EDeltaPolicy::Skip"));
		static const FString InvalidOrMax(TEXT("InvalidOrMax"));

		switch (DeltaPolicy)
		{
			case EDeltaPolicy::TryFetchContinueWithout: return TryFetchContinueWithout;
			case EDeltaPolicy::Expect: return Expect;
			case EDeltaPolicy::Skip: return Skip;
			default: return InvalidOrMax;
		}
	}

	/**
	 * Based on the source and destination manifests, get the filename for the delta that optimises patching from source to destination.
	 * @param SourceManifest        The source manifest.
	 * @param DestinationManifest   The destination manifest.
	 * @return the CloudDir relative delta filename.
	 */
	BUILDPATCHSERVICES_API FString GetChunkDeltaFilename(const IBuildManifestRef& SourceManifest, const IBuildManifestRef& DestinationManifest);

	BUILDPATCHSERVICES_API IBuildManifestPtr MergeDeltaManifest(const IBuildManifestRef& Manifest, const IBuildManifestRef& Delta);
}
