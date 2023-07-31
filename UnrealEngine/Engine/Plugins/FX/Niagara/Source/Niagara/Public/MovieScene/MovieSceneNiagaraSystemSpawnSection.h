// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "MovieSceneSection.h"
#include "MovieSceneNiagaraSystemSpawnSection.generated.h"

/** Defines options for system life cycle for before the section is evaluating up to the first frame the section evaluates. */
UENUM()
enum class ENiagaraSystemSpawnSectionStartBehavior
{
	/** When the time before the section evaluates the particle system's component will be deactivated and on the first frame of the section the
	 system's component will be activated. */
	Activate
};

/** Defines options for system life cycle for when the section is evaluating from the 2nd frame until the last frame of the section. */
UENUM()
enum class ENiagaraSystemSpawnSectionEvaluateBehavior
{
	/** The system's component will be activated on any frame where it is inactive.  This is useful for continuous emitters, especially if the sequencer will start in the middle of the section. */
	ActivateIfInactive,
	/** There sill be no changes to the system life cycle while the section is evaluating. */
	None
};

/** Defines options for system life cycle for the time after the section. */
UENUM()
enum class ENiagaraSystemSpawnSectionEndBehavior
{
	//** When the section ends the system is set to inactive which stops spawning but lets existing particles simulate until death.
	SetSystemInactive,
	//** When the section ends the system's component is deactivated which will kill all existing particles.
	Deactivate,
	//** Does nothing when the section ends and allows the system to continue to run as normal.
	None
};

UCLASS(MinimalAPI)
class UMovieSceneNiagaraSystemSpawnSection : public UMovieSceneSection
{
	GENERATED_BODY()

public:
	UMovieSceneNiagaraSystemSpawnSection();

	ENiagaraSystemSpawnSectionStartBehavior GetSectionStartBehavior() const;

	ENiagaraSystemSpawnSectionEvaluateBehavior GetSectionEvaluateBehavior() const;

	ENiagaraSystemSpawnSectionEndBehavior GetSectionEndBehavior() const;

	ENiagaraAgeUpdateMode GetAgeUpdateMode() const;

	bool GetAllowScalability()const;

private:
	/** Specifies what should happen to the niagara system from before the section evaluates up until the first frame of the section. */
	UPROPERTY(EditAnywhere, Category = "Life Cycle")
	ENiagaraSystemSpawnSectionStartBehavior SectionStartBehavior;

	/** Specifies what should happen to the niagara system when section is evaluating from the 2nd frame until the last frame. */
	UPROPERTY(EditAnywhere, Category = "Life Cycle")
	ENiagaraSystemSpawnSectionEvaluateBehavior SectionEvaluateBehavior;

	/** Specifies what should happen to the niagara system when section evaluation finishes. */
	UPROPERTY(EditAnywhere, Category = "Life Cycle")
	ENiagaraSystemSpawnSectionEndBehavior SectionEndBehavior;

	/** Specifies how sequencer should update the age of the controlled niagara system. */
	UPROPERTY(EditAnywhere, Category = "Life Cycle")
	ENiagaraAgeUpdateMode AgeUpdateMode;
	
	UPROPERTY(EditAnywhere, Category = "Life Cycle")
	bool bAllowScalability;
};