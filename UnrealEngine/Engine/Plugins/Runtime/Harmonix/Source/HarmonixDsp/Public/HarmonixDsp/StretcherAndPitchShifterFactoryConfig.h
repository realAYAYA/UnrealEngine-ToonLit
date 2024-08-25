// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixDsp/PitchShifterName.h"
#include "Harmonix/HarmonixDeveloperSettings.h"

#include "StretcherAndPitchShifterFactoryConfig.generated.h"

USTRUCT()
struct FPitchShifterNameRedirect
{
	GENERATED_BODY()

	FPitchShifterNameRedirect() 
	{}
	
	FPitchShifterNameRedirect(FName OldName, FName NewName)
		: OldName(OldName), NewName(NewName) 
	{}

	UPROPERTY(EditDefaultsOnly, Category = "Factory")
	FName OldName;

	UPROPERTY(EditDefaultsOnly, Category = "Factory")
	FName NewName;
};

UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Pitch Shifter Settings"))
class HARMONIXDSP_API UStretcherAndPitchShifterFactoryConfig : public UHarmonixDeveloperSettings
{
	GENERATED_BODY()

public:
	UStretcherAndPitchShifterFactoryConfig();

	UPROPERTY(config, EditDefaultsOnly, Category = "Factory")
	TArray<FName> FactoryPriority;

	UPROPERTY(config, EditDefaultsOnly, Category = "Factory")
	FPitchShifterName DefaultFactory;

	void AddFactoryNameRedirect(FName OldName, FName NewName);

	const FPitchShifterNameRedirect* FindFactoryNameRedirect(FName OldName) const;

public:

#if WITH_EDITORONLY_DATA

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif

	virtual void PostInitProperties() override;

private:

	void PruneDuplicatesAndAddMissingNames(TArray<FName>& InOutNames);

	// visible in editor, but not editable
	UPROPERTY(config, EditDefaultsOnly, Category = "Factory", Meta=(EditCondition="false"))
	TArray<FPitchShifterNameRedirect> FactoryNameRedirects;
};