// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineConsoleVariableSetting.h"
#include "MovieRenderPipelineCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineConsoleVariableSetting)

namespace UE
{
	namespace MoviePipeline
	{
		static void SetValue(IConsoleVariable* InCVar, float InValue)
		{
			check(InCVar);

			// When Set is called on a cvar the value is turned into a string. With very large
			// floats this is turned into scientific notation. If the cvar is later retrieved as
			// an integer, the scientific notation doesn't parse into integer correctly. We'll
			// cast to integer first (to avoid scientific notation) if we know the cvar is an integer.
			if (InCVar->IsVariableInt())
			{
				InCVar->Set(static_cast<int32>(InValue), EConsoleVariableFlags::ECVF_SetByConsole);
			}
			else if (InCVar->IsVariableBool())
			{
				InCVar->Set(InValue != 0.f ? true : false, EConsoleVariableFlags::ECVF_SetByConsole);
			}
			else
			{
				InCVar->Set(InValue, EConsoleVariableFlags::ECVF_SetByConsole);
			}
		}
	}
}

void UMoviePipelineConsoleVariableSetting::SetupForPipelineImpl(UMoviePipeline* InPipeline)
{
	ApplyCVarSettings(true);
}

void UMoviePipelineConsoleVariableSetting::TeardownForPipelineImpl(UMoviePipeline* InPipeline)
{
	ApplyCVarSettings(false);
}
	
void UMoviePipelineConsoleVariableSetting::ApplyCVarSettings(const bool bOverrideValues)
{
	if (bOverrideValues)
	{
		PreviousConsoleVariableValues.Reset();
		PreviousConsoleVariableValues.SetNumZeroed(ConsoleVariables.Num());
	}

	int32 Index = 0;
	for(const TPair<FString, float>& KVP : ConsoleVariables)
	{
		// We don't use the shared macro here because we want to soft-warn the user instead of tripping an ensure over missing cvar values.
		const FString TrimmedCvar = KVP.Key.TrimStartAndEnd();
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*TrimmedCvar); 
		if (CVar)
		{
			if (bOverrideValues)
			{
				PreviousConsoleVariableValues[Index] = CVar->GetFloat();
				UE::MoviePipeline::SetValue(CVar, KVP.Value);
				UE_LOG(LogMovieRenderPipeline, Log, TEXT("Applying CVar \"%s\" PreviousValue: %f NewValue: %f"), *KVP.Key, PreviousConsoleVariableValues[Index], KVP.Value);
			}
			else
			{
				UE::MoviePipeline::SetValue(CVar, PreviousConsoleVariableValues[Index]);
				UE_LOG(LogMovieRenderPipeline, Log, TEXT("Restoring CVar \"%s\" PreviousValue: %f NewValue: %f"), *KVP.Key, KVP.Value, PreviousConsoleVariableValues[Index]);
			}
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Failed to apply CVar \"%s\" due to no cvar by that name. Ignoring."), *KVP.Key);
		}

		Index++;
	}

	if (bOverrideValues)
	{
		for (const FString& Command : StartConsoleCommands)
		{
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Executing Console Command \"%s\" before shot starts."), *Command);
			UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), Command, nullptr);
		}
	}
	else
	{
		for (const FString& Command : EndConsoleCommands)
		{
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Executing Console Command \"%s\" after shot ends."), *Command);
			UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), Command, nullptr);
		}
	}
}

