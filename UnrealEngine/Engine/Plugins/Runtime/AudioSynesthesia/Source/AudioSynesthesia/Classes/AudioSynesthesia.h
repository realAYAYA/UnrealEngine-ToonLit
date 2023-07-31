// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioAnalyzer.h"
#include "AudioSynesthesia.generated.h"

/** UAudioSynesthesiaSettings
 *
 * Defines asset actions for derived UAudioSynthesiaSettings subclasses.
 */
UCLASS(Abstract, Blueprintable)
class AUDIOSYNESTHESIA_API UAudioSynesthesiaSettings : public UAudioAnalyzerSettings
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	const TArray<FText>& GetAssetActionSubmenus() const;
	FColor GetTypeColor() const override;
#endif
};