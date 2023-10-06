// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioAnalyzerNRT.h"
#include "AudioSynesthesiaNRT.generated.h"

/** UAudioSynesthesiaNRTSettings
 *
 * Defines asset actions for derived UAudioSynthesiaNRTSettings subclasses.
 */
UCLASS(Abstract, Blueprintable)
class AUDIOSYNESTHESIA_API UAudioSynesthesiaNRTSettings : public UAudioAnalyzerNRTSettings
{
	GENERATED_BODY()

	public:

		const TArray<FText>& GetAssetActionSubmenus() const;

#if WITH_EDITOR
		FColor GetTypeColor() const override;
#endif
};

/** UAudioSynesthesiaNRT
 *
 * Defines asset actions for derived UAudioSynthesiaNRT subclasses.
 */
UCLASS(Abstract, Blueprintable)
class AUDIOSYNESTHESIA_API UAudioSynesthesiaNRT : public UAudioAnalyzerNRT
{
	GENERATED_BODY()

	public:

		const TArray<FText>& GetAssetActionSubmenus() const;

#if WITH_EDITOR
		FColor GetTypeColor() const override;
#endif
};

