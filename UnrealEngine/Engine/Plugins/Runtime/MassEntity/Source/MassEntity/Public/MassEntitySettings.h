// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassSettings.h"
#include "MassProcessingPhaseManager.h"
#include "MassProcessor.h"
#include "InstancedStruct.h"
#include "MassEntitySettings.generated.h"


#define GET_MASS_CONFIG_VALUE(a) (GetMutableDefault<UMassEntitySettings>()->a)

class UMassProcessingPhaseManager;
struct FMassProcessingPhaseConfig;
struct FPropertyChangedEvent;


USTRUCT()
struct FMassProcessingPhaseConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Mass, config)
	FName PhaseName;

	UPROPERTY(EditAnywhere, Category = Mass, config, NoClear)
	TSubclassOf<UMassCompositeProcessor> PhaseGroupClass = UMassCompositeProcessor::StaticClass();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMassProcessor>> ProcessorCDOs;

#if WITH_EDITORONLY_DATA
	// this processor is available only in editor since it's used to present the user the order in which processors
	// will be executed when given processing phase gets triggered
	UPROPERTY(Transient)
	TObjectPtr<UMassCompositeProcessor> PhaseProcessor = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Mass, Transient)
	FText Description;
#endif //WITH_EDITORONLY_DATA
};

/**
 * Implements the settings for MassEntity plugin
 */
UCLASS(config = Mass, defaultconfig, DisplayName = "Mass Entity")
class MASSENTITY_API UMassEntitySettings : public UMassModuleSettings
{
	GENERATED_BODY()
public:
#if WITH_EDITORONLY_DATA
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingsChange, const FPropertyChangedEvent& /*PropertyChangedEvent*/);
#endif // WITH_EDITORONLY_DATA

	UMassEntitySettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void BuildProcessorListAndPhases();
	void AddToActiveProcessorsList(TSubclassOf<UMassProcessor> ProcessorClass);

	const FMassProcessingPhaseConfig* GetProcessingPhasesConfig();
	const FMassProcessingPhaseConfig& GetProcessingPhaseConfig(const EMassProcessingPhase ProcessingPhase) const { check(ProcessingPhase != EMassProcessingPhase::MAX); return ProcessingPhasesConfig[int(ProcessingPhase)]; }

#if WITH_EDITOR
	FOnSettingsChange& GetOnSettingsChange() { return OnSettingsChange; }	

protected:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual void BeginDestroy() override;

	void BuildPhases();
	void BuildProcessorList();

public:
	/** 
	 * The name of the file to dump the processor dependency graph. T
	 * The dot file will be put in the project log folder.
	 * To generate a svg out of that file, simply run dot executable with following parameters: -Tsvg -O filename.dot 
	 */
	UPROPERTY(EditDefaultsOnly, Category = Mass, Transient)
	FString DumpDependencyGraphFileName;

	/** Lets users configure processing phases including the composite processor class to be used as a container for the phases' processors. */
	UPROPERTY(EditDefaultsOnly, Category = Mass, config)
	FMassProcessingPhaseConfig ProcessingPhasesConfig[(uint8)EMassProcessingPhase::MAX];

	/** This list contains all the processors available in the given binary (including plugins). The contents are sorted by display name.*/
	UPROPERTY(VisibleAnywhere, Category = Mass, Transient, Instanced, EditFixedSize)
	TArray<TObjectPtr<UMassProcessor>> ProcessorCDOs;

#if WITH_EDITORONLY_DATA
protected:
	FOnSettingsChange OnSettingsChange;
#endif // WITH_EDITORONLY_DATA
	bool bInitialized = false;
};
