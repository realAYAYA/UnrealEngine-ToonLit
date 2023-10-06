// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineConsoleVariableSetting.h"

#include "MoviePipelineQueue.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/DefaultValueHelper.h"
#include "Sections/MovieSceneConsoleVariableTrackInterface.h"
#include "Sections/MovieSceneCVarSection.h"
#include "Tracks/MovieSceneCVarTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineConsoleVariableSetting)

#define LOCTEXT_NAMESPACE "MoviePipelineConsoleVariableSetting"

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
				InCVar->SetWithCurrentPriority(static_cast<int32>(InValue));
			}
			else if (InCVar->IsVariableBool())
			{
				InCVar->SetWithCurrentPriority(InValue != 0.f ? true : false);
			}
			else
			{
				InCVar->SetWithCurrentPriority(InValue);
			}
		}

		/**
		 * Determine if the given UMovieScene contains any CVar tracks that are not muted, and also have an
		 * active section with CVars that are set. Sub-sequences will be searched for CVar tracks as well.
		 */
		static bool IsCVarTrackPresent(const UMovieScene* InMovieScene)
		{
			if (!InMovieScene)
			{
				return false;
			}

			for (UMovieSceneTrack* Track : InMovieScene->GetTracks())
			{
				// Process CVar tracks. Return immediately if any of the CVar tracks contain CVars that are set.
				// If this is the case, sub tracks don't need to be searched.
				if (Track->IsA<UMovieSceneCVarTrack>())
				{
					const UMovieSceneCVarTrack* CVarTrack = Cast<UMovieSceneCVarTrack>(Track);
					for (const UMovieSceneSection* Section : CVarTrack->GetAllSections())
					{
						const UMovieSceneCVarSection* CVarSection = Cast<UMovieSceneCVarSection>(Section);
						if (!CVarSection || !MovieSceneHelpers::IsSectionKeyable(CVarSection))
						{
							continue;
						}
						
						// Does this CVar track have anything in it?
						if (!CVarSection->ConsoleVariableCollections.IsEmpty() || !CVarSection->ConsoleVariables.ValuesByCVar.IsEmpty())
						{
							return true;
						}
					}
				}
				
				// Process sub tracks (which could potentially contain other sequences with CVar tracks)
				if (Track->IsA<UMovieSceneSubTrack>())
				{
					const UMovieSceneSubTrack* SubTrack = Cast<UMovieSceneSubTrack>(Track);
					for (const UMovieSceneSection* Section : SubTrack->GetAllSections())
					{
						const UMovieSceneSubSection* SubSection = Cast<UMovieSceneSubSection>(Section);
						if (!SubSection || !MovieSceneHelpers::IsSectionKeyable(SubSection))
						{
							continue;
						}

						// Recurse into sub-sequences
						if (const UMovieSceneSequence* SubSequence = SubSection->GetSequence())
						{
							if (IsCVarTrackPresent(SubSequence->GetMovieScene()))
							{
								return true;
							}
						}
					}
				}
			}
			
			return false;
		}
	}
}

#if WITH_EDITOR

FText UMoviePipelineConsoleVariableSetting::GetFooterText(UMoviePipelineExecutorJob* InJob) const
{
	if (!InJob)
	{
		return FText();
	}
	
	const ULevelSequence* LoadedSequence = Cast<ULevelSequence>(InJob->Sequence.TryLoad());
	if (!LoadedSequence)
	{
		return FText();
	}
	
	if (!UE::MoviePipeline::IsCVarTrackPresent(LoadedSequence->MovieScene))
	{
		return FText();
	}
	
	return FText(LOCTEXT(
		"SequencerCvarWarning",
		"The current job contains a Level Sequence with a Console Variables Track, additional settings are configured in Sequencer."));
}

FString UMoviePipelineConsoleVariableSetting::ResolvePresetValue(const FString& InCVarName) const
{
	FString ResolvedValue;

	// Iterate the presets in reverse; cvars in presets at the end of the array take precedence.
	for (int32 Index = ConsoleVariablePresets.Num() - 1; Index >= 0; --Index)
	{
		const TScriptInterface<IMovieSceneConsoleVariableTrackInterface>& Preset = ConsoleVariablePresets[Index];
		if (!Preset)
		{
			continue;
		}
		
		const bool bOnlyIncludeChecked = true;
		TArray<TPair<FString, FString>> PresetCVars;
		Preset->GetConsoleVariablesForTrack(bOnlyIncludeChecked, PresetCVars);
		
		for (const TPair<FString, FString>& CVar : PresetCVars)
		{
			if (CVar.Key.Equals(InCVarName))
			{
				// Found a matching cvar in the preset
				return CVar.Value;
			}
		}
	}

	return FString();
}

FString UMoviePipelineConsoleVariableSetting::ResolveDisabledValue(const FMoviePipelineConsoleVariableEntry& InEntry) const
{
	// There may be multiple cvar overrides w/ the same name, so even if the provided entry is disabled, we still need
	// to look through the other entries for cvars with the same name. Iterate in reverse, since cvars at the end of the
	// array take precedence.
	for (int32 Index = CVars.Num() - 1; Index >= 0; --Index)
	{
		const FMoviePipelineConsoleVariableEntry& CVarEntry = CVars[Index];
		if (CVarEntry.Name.Equals(InEntry.Name) && CVarEntry.bIsEnabled)
		{
			return FString::SanitizeFloat(CVarEntry.Value);
		}
	}

	// If no override value was found, look for a value from the presets
	const FString PresetValue = ResolvePresetValue(InEntry.Name);
	if (!PresetValue.IsEmpty())
	{
		return PresetValue;
	}

	// Fall back to the startup value of the cvar
	const TSharedPtr<FConsoleVariablesEditorCommandInfo> CommandInfo = InEntry.CommandInfo.Pin();
	if (CommandInfo.IsValid())
	{
		return CommandInfo->StartupValueAsString;
	}

	return FString();
}

FMoviePipelineConsoleVariableEntry* UMoviePipelineConsoleVariableSetting::GetCVarAtIndex(const int32 InIndex)
{
	if (!CVars.IsValidIndex(InIndex))
	{
		return nullptr;
	}

	return &CVars[InIndex];
}

#endif // WITH_EDITOR

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
		MergeInOldConsoleVariables();
		MergeInPresetConsoleVariables();
		PreviousConsoleVariableValues.Reset();
		PreviousConsoleVariableValues.SetNumZeroed(MergedConsoleVariables.Num());
	}

	int32 Index = 0;
	for(const FMoviePipelineConsoleVariableEntry& CVarEntry : MergedConsoleVariables)
	{
		// We don't use the shared macro here because we want to soft-warn the user instead of tripping an ensure over missing cvar values.
		const FString TrimmedCvar = CVarEntry.Name.TrimStartAndEnd();
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*TrimmedCvar); 
		if (CVar)
		{
			if (bOverrideValues)
			{
				PreviousConsoleVariableValues[Index] = CVar->GetFloat();
				UE::MoviePipeline::SetValue(CVar, CVarEntry.Value);
				UE_LOG(LogMovieRenderPipeline, Log, TEXT("Applying CVar \"%s\" PreviousValue: %f NewValue: %f"), *CVarEntry.Name, PreviousConsoleVariableValues[Index], CVarEntry.Value);
			}
			else
			{
				UE::MoviePipeline::SetValue(CVar, PreviousConsoleVariableValues[Index]);
				UE_LOG(LogMovieRenderPipeline, Log, TEXT("Restoring CVar \"%s\" PreviousValue: %f NewValue: %f"), *CVarEntry.Name, CVarEntry.Value, PreviousConsoleVariableValues[Index]);
			}
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Failed to apply CVar \"%s\" due to no cvar by that name. Ignoring."), *CVarEntry.Name);
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

TArray<FMoviePipelineConsoleVariableEntry> UMoviePipelineConsoleVariableSetting::GetConsoleVariables() const
{
	return CVars;
}

bool UMoviePipelineConsoleVariableSetting::RemoveConsoleVariable(const FString& Name, const bool bRemoveAllInstances)
{
	if (bRemoveAllInstances)
	{
		const int32 NumRemoved = CVars.RemoveAll([&Name](const FMoviePipelineConsoleVariableEntry& Entry)
		{
			return Entry.Name.Equals(Name);
		});

		return NumRemoved != 0;
	}

	const int32 LastMatch = CVars.FindLastByPredicate([&Name](const FMoviePipelineConsoleVariableEntry& Entry)
	{
		return Entry.Name.Equals(Name);
	});

	if (LastMatch != INDEX_NONE)
	{
		CVars.RemoveAt(LastMatch);
		return true;
	}

	return false;
}

bool UMoviePipelineConsoleVariableSetting::AddOrUpdateConsoleVariable(const FString& Name, const float Value)
{
	const int32 LastMatch = CVars.FindLastByPredicate([&Name](const FMoviePipelineConsoleVariableEntry& Entry)
	{
		return Entry.Name.Equals(Name);
	});

	if (LastMatch != INDEX_NONE)
	{
		CVars[LastMatch].Value = Value;
		return true;
	}

	CVars.Add(FMoviePipelineConsoleVariableEntry(Name, Value));

	return true;
}

bool UMoviePipelineConsoleVariableSetting::AddConsoleVariable(const FString& Name, const float Value)
{
	CVars.Add(FMoviePipelineConsoleVariableEntry(Name, Value));

	return true;
}

bool UMoviePipelineConsoleVariableSetting::UpdateConsoleVariableEnableState(const FString& Name, const bool bIsEnabled)
{
	const int32 LastMatch = CVars.FindLastByPredicate([&Name](const FMoviePipelineConsoleVariableEntry& Entry)
	{
		return Entry.Name.Equals(Name);
	});

	if (LastMatch != INDEX_NONE)
	{
		CVars[LastMatch].bIsEnabled = bIsEnabled;
		return true;
	}

	return false;
}

void UMoviePipelineConsoleVariableSetting::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	// Convert the old ConsoleVariables map into the new struct array
	if (!ConsoleVariables_DEPRECATED.IsEmpty())
	{
		for (const TPair<FString, float>& Pair : ConsoleVariables_DEPRECATED)
		{
			// The converted cvar will be enabled by default
			CVars.Add(FMoviePipelineConsoleVariableEntry(Pair.Key, Pair.Value));
		}

		ConsoleVariables_DEPRECATED.Empty();
	}
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Initialize the CommandInfo structs
	for (FMoviePipelineConsoleVariableEntry& Entry : CVars)
	{
		Entry.UpdateCommandInfo();
	}
#endif	// WITH_EDITOR
}

#if WITH_EDITOR
void UMoviePipelineConsoleVariableSetting::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FProperty* Property = PropertyChangedEvent.Property;
	const FProperty* MemberProperty = nullptr;
	if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
	{
		MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
	}

	if (!MemberProperty || !Property)
	{
		return;
	}

	// If the name of one of the cvar overrides changes, generate a new CommandInfo for it
	if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UMoviePipelineConsoleVariableSetting, CVars))
	{
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(FMoviePipelineConsoleVariableEntry, Name))
		{
			const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(MemberProperty->GetName());
			if (CVars.IsValidIndex(ArrayIndex))
			{
				CVars[ArrayIndex].UpdateCommandInfo();
			}
		}
	}
}
#endif	// WITH_EDITOR

void UMoviePipelineConsoleVariableSetting::MergeInOldConsoleVariables()
{
	// Note: The old cvars in ConsoleVariables are merged in via PostLoad(), which covers most use cases.
	// However, scripting may have modified ConsoleVariables after initial load, so they need to be
	// merged in again in certain scenarios, like when cvars are applied.
	//
	// Data loss is possible if scripting modified ConsoleVariables, but this method never executes
	// (ie, MRQ never gets run in PIE). ConsoleVariables is deprecated, thus changes to it will not be saved.
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	for (const TPair<FString, float>& OldCVar : ConsoleVariables_DEPRECATED)
	{
		// Copy the value in ConsoleVariables over to CVars if an entry in CVars already exists. Apply the value to the
		// last matching entry in CVars, since the last entry will win.
		bool bDidFindNewCVar = false;
		for (int32 Index = CVars.Num() - 1; Index >= 0; --Index)
		{
			FMoviePipelineConsoleVariableEntry& NewCVar = CVars[Index];
			if (NewCVar.Name == OldCVar.Key)
			{
				NewCVar.Value = OldCVar.Value;
				bDidFindNewCVar = true;
				break;
			}
		}

		// Otherwise, add a new entry to CVars
		if (!bDidFindNewCVar)
		{
			CVars.Add(FMoviePipelineConsoleVariableEntry(OldCVar.Key, OldCVar.Value));
		}
	}
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UMoviePipelineConsoleVariableSetting::MergeInPresetConsoleVariables()
{
	MergedConsoleVariables.Reset();
	
	// Merge in the presets
	for (const TScriptInterface<IMovieSceneConsoleVariableTrackInterface>& Preset : ConsoleVariablePresets)
	{
		if (!Preset)
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Invalid CVar preset specified. Ignoring."));
			continue;
		}
		
		const bool bOnlyIncludeChecked = true;
		TArray<TTuple<FString, FString>> PresetCVars;
		Preset->GetConsoleVariablesForTrack(bOnlyIncludeChecked, PresetCVars);
		
		for (const TTuple<FString, FString>& CVarPair : PresetCVars)
		{
			float CVarFloatValue = 0.0f;
			if (FDefaultValueHelper::ParseFloat(CVarPair.Value, CVarFloatValue))
			{
				MergedConsoleVariables.Add(FMoviePipelineConsoleVariableEntry(CVarPair.Key, CVarFloatValue));
			}
			else
			{
				UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Failed to apply CVar \"%s\" (from preset \"%s\") because value could not be parsed into a float. Ignoring."),
					*CVarPair.Key, *Preset.GetObject()->GetName());
			}
		}
	}
	
	// Merge in the overrides
	for (const FMoviePipelineConsoleVariableEntry& Entry : CVars)
	{
		if (!Entry.bIsEnabled)
		{
			continue;
		}
		
		MergedConsoleVariables.Add(Entry);
	}
}

#undef LOCTEXT_NAMESPACE