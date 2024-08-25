// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphCVarManager.h"

#include "Graph/MovieGraphConfig.h"
#include "Graph/Nodes/MovieGraphApplyCVarPresetNode.h"
#include "Graph/Nodes/MovieGraphSetCVarValueNode.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DefaultValueHelper.h"
#include "MovieRenderPipelineCoreModule.h"
#include "Sections/MovieSceneConsoleVariableTrackInterface.h"

namespace UE::MovieGraph::Private
{

void FMovieGraphCVarManager::AddCVar(const FString& InName, const float Value)
{
	FMovieGraphCVarOverride* CVarOverride = CVars.FindByPredicate([&InName](const FMovieGraphCVarOverride& Override)
	{
		return Override.Name == InName;
	});

	if (CVarOverride)
	{
		CVarOverride->Value = Value;
	}
	else
	{
		CVars.Add(FMovieGraphCVarOverride(InName, Value));
	}
}

void FMovieGraphCVarManager::AddPreset(const TScriptInterface<IMovieSceneConsoleVariableTrackInterface>& InPreset)
{
	if (!InPreset)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Invalid CVar preset specified. Ignoring."));
		return;
	}

	constexpr bool bOnlyIncludeChecked = true;
	TArray<TTuple<FString, FString>> PresetCVars;
	InPreset->GetConsoleVariablesForTrack(bOnlyIncludeChecked, PresetCVars);

	for (const TTuple<FString, FString>& CVarPair : PresetCVars)
	{
		float CVarFloatValue = 0.0f;
		if (FDefaultValueHelper::ParseFloat(CVarPair.Value, CVarFloatValue))
		{
			AddCVar(CVarPair.Key, CVarFloatValue);
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Failed to apply CVar \"%s\" (from preset \"%s\") because value could not be parsed into a float. Ignoring."),
				*CVarPair.Key, *InPreset.GetObject()->GetName());
		}
	}
}

void FMovieGraphCVarManager::AddEvaluatedGraph(const UMovieGraphEvaluatedConfig* InEvaluatedGraph)
{
	const FMovieGraphEvaluatedBranchConfig* GlobalsBranchConfig = InEvaluatedGraph->BranchConfigMapping.Find(UMovieGraphNode::GlobalsPinName);
	if (!GlobalsBranchConfig)
	{
		return;
	}

	// Nodes which set cvars can only be created in the Globals branch, so we only need to look for nodes in that branch
	const TArray<TObjectPtr<UMovieGraphNode>> GlobalsNodes = GlobalsBranchConfig->GetNodes();

	// Iterate the evaluated graph in reverse. Nodes which come first in the array appear furthest downstream in the
	// graph, so they should be applied last.
	for (int32 NodeIndex = GlobalsNodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
	{
		const TObjectPtr<UMovieGraphNode>& Node = GlobalsNodes[NodeIndex];

		if (const UMovieGraphSetCVarValueNode* CVarValueNode = Cast<UMovieGraphSetCVarValueNode>(Node))
		{
			AddCVar(CVarValueNode->Name, CVarValueNode->Value);
		}
		else if (const UMovieGraphApplyCVarPresetNode* ApplyPresetNode = Cast<UMovieGraphApplyCVarPresetNode>(Node))
		{
			AddPreset(ApplyPresetNode->ConsoleVariablePreset);
		}
	}
}

void FMovieGraphCVarManager::ApplyAllCVars()
{
	PreviousConsoleVariableValues.Reset();
	PreviousConsoleVariableValues.SetNumZeroed(CVars.Num());

	for (int32 CVarIndex = 0; CVarIndex < CVars.Num(); ++CVarIndex)
	{
		const FMovieGraphCVarOverride& CVarEntry = CVars[CVarIndex];
		const FString TrimmedCvar = CVarEntry.Name.TrimStartAndEnd();
	
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*TrimmedCvar))
		{
			PreviousConsoleVariableValues[CVarIndex] = CVar->GetFloat();
			ApplyCVar(CVar, CVarEntry.Value);
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Applying CVar \"%s\", PreviousValue: %f NewValue: %f"), *CVarEntry.Name, PreviousConsoleVariableValues[CVarIndex], CVarEntry.Value);
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Failed to apply CVar \"%s\" due to no cvar by that name. Ignoring."), *CVarEntry.Name);
		}
	}
}

void FMovieGraphCVarManager::RevertAllCVars()
{
	for (int32 CVarIndex = 0; CVarIndex < CVars.Num(); ++CVarIndex)
	{
		const FMovieGraphCVarOverride& CVarEntry = CVars[CVarIndex];
		const FString TrimmedCvar = CVarEntry.Name.TrimStartAndEnd();
	
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*TrimmedCvar))
		{
			ApplyCVar(CVar, PreviousConsoleVariableValues[CVarIndex]);
			UE_LOG(LogMovieRenderPipeline, Log, TEXT("Restoring CVar \"%s\", PreviousValue: %f NewValue: %f"), *CVarEntry.Name, CVarEntry.Value, PreviousConsoleVariableValues[CVarIndex]);
		}
		else
		{
			UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Failed to revert CVar \"%s\" due to no cvar by that name. Ignoring."), *CVarEntry.Name);
		}
	}

	CVars.Reset();
	PreviousConsoleVariableValues.Reset();
}

void FMovieGraphCVarManager::ApplyCVar(IConsoleVariable* InCVar, float InValue)
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

} // namespace UE::MovieGraph::Private