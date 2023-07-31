// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineSetting.h"
#include "MoviePipelineConsoleVariableSetting.generated.h"

UCLASS(BlueprintType)
class MOVIERENDERPIPELINESETTINGS_API UMoviePipelineConsoleVariableSetting : public UMoviePipelineSetting
{
	GENERATED_BODY()
public:

public:
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "ConsoleVariableSettingDisplayName", "Console Variables"); }
#endif
	virtual bool IsValidOnShots() const override { return true; }
	virtual bool IsValidOnMaster() const override { return true; }
	virtual void SetupForPipelineImpl(UMoviePipeline* InPipeline) override;
	virtual void TeardownForPipelineImpl(UMoviePipeline* InPipeline) override;
protected:
	void ApplyCVarSettings(const bool bOverrideValues);

public:
	/** 
	* An array of key/value pairs for console variable name and the value you wish to set for that cvar.
	* The existing value will automatically be cached and restored afterwards.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TMap<FString, float> ConsoleVariables;

	/**
	* An array of console commands to execute when this shot is started. If you need to restore the value 
	* after the shot, add a matching entry in the EndConsoleCommands array. Because they are commands
	* and not values we cannot save the preivous value automatically.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FString> StartConsoleCommands;

	/**
	* An array of console commands to execute when this shot is finished. Used to restore changes made by
	* StartConsoleCommands.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FString> EndConsoleCommands;

private:
	TArray<float> PreviousConsoleVariableValues;
};
