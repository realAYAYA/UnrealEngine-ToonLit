// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundGeneratorHandle.h"
#include "Subsystems/WorldSubsystem.h"

#include "MetasoundOutputSubsystem.generated.h"

class UAudioComponent;
class UMetasoundGeneratorHandle;

/**
 * Provides access to a playing Metasound generator's outputs
 */
UCLASS()
class METASOUNDENGINE_API UMetaSoundOutputSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual ~UMetaSoundOutputSubsystem() override = default;
	
	/**
	 * Watch an output on a Metasound playing on a given audio component.
	 *
	 * @param AudioComponent - The audio component
	 * @param OutputName - The user-specified name of the output in the Metasound
	 * @param OnOutputValueChanged - The event to fire when the output's value changes
	 * @param AnalyzerName - (optional) The name of the analyzer to use on the output, defaults to a passthrough
	 * @param AnalyzerOutputName - (optional) The name of the output on the analyzer to watch, defaults to the passthrough output
	 * @returns true if the watch setup succeeded, false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category="MetaSoundOutput", meta=(AdvancedDisplay = "3"))
	bool WatchOutput(
		UAudioComponent* AudioComponent,
		FName OutputName,
		const FOnMetasoundOutputValueChanged& OnOutputValueChanged,
		FName AnalyzerName = NAME_None,
		FName AnalyzerOutputName = NAME_None);

	bool WatchOutput(
		UAudioComponent* AudioComponent,
		FName OutputName,
		const FOnMetasoundOutputValueChangedNative& OnOutputValueChanged,
		FName AnalyzerName = NAME_None,
		FName AnalyzerOutputName = NAME_None);
	
	/** Begin UTickableWorldSubsystem */
	virtual bool IsTickable() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	/** End UTickableWorldSubsystem */

private:
	UMetasoundGeneratorHandle* GetOrCreateGeneratorHandle(UAudioComponent* AudioComponent);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMetasoundGeneratorHandle>> TrackedGenerators;
};
