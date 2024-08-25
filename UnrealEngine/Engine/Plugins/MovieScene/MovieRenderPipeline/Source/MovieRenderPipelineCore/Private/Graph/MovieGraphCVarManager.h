// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/ScriptInterface.h"

class IMovieSceneConsoleVariableTrackInterface;
class UMovieGraphEvaluatedConfig;
class UMovieScene;

namespace UE::MovieGraph::Private
{
	/**
	 * Responsible for applying individual cvars and cvar presets. CVars can be added to the manager with the Add*() methods,
	 * then applied via ApplyAllCVars().
	 */
	class FMovieGraphCVarManager final
	{
	public:
		FMovieGraphCVarManager() = default;

		/**
		 * Adds a cvar that should be applied with the given Value. If the cvar with InName already exists, its value
		 * will be updated.
		 */
		void AddCVar(const FString& InName, const float Value);

		/**
		 * Adds a preset that should be applied. If the preset contains cvars that have already been added via other Add*()
		 * calls, the values in the preset will override values already added.
		 */
		void AddPreset(const TScriptInterface<IMovieSceneConsoleVariableTrackInterface>& InPreset);

		/** For all cvar and cvar preset nodes in InEvaluatedGraph, calls either AddCVar() or AddPreset(). */
		void AddEvaluatedGraph(const UMovieGraphEvaluatedConfig* InEvaluatedGraph);

		/** Applies all cvars that have been gathered via the Add*() methods. */
		void ApplyAllCVars();

		/**
		 * For all cvars that were gathered via the Add*() methods and applied, reverts their values to what they were before
		 * being applied. After calling this, ApplyAllCVars() is a no-op, and the Add*() methods need to be called again.
		 */
		void RevertAllCVars();

	private:
		/** Sets the given cvar, InCVar, to InValue. */
		static void ApplyCVar(IConsoleVariable* InCVar, float InValue);

		/** Represents a console variable override. */
		struct FMovieGraphCVarOverride
		{
			FMovieGraphCVarOverride(const FString& InName, const float InValue)
				: Name(InName)
				, Value(InValue)
			{
			}

			/* The name of the console variable. */
			FString Name;

			/* The value of the console variable. */
			float Value;
		};

	private:
		/** The merged result of both individual CVars and CVar presets. */
		TArray<FMovieGraphCVarOverride> CVars;

		/** The values of the gathered cvars before the the manager sets their values. */
		TArray<float> PreviousConsoleVariableValues;
	};
} // namespace UE::MovieGraph::Private