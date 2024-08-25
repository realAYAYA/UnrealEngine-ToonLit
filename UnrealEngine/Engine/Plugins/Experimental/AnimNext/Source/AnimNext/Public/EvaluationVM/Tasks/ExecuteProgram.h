// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EvaluationVM/EvaluationTask.h"

#include "ExecuteProgram.generated.h"

namespace UE::AnimNext
{
	struct FEvaluationProgram;
}

/*
 * Execute Program Task
 *
 * This allows external caching of evaluation programs by deferring evaluation
 * or repeated evaluations.
 */
USTRUCT()
struct ANIMNEXT_API FAnimNextExecuteProgramTask : public FAnimNextEvaluationTask
{
	GENERATED_BODY()

	DECLARE_ANIM_EVALUATION_TASK(FAnimNextExecuteProgramTask)

	static FAnimNextExecuteProgramTask Make(const UE::AnimNext::FEvaluationProgram* Program);

	// Task entry point
	virtual void Execute(UE::AnimNext::FEvaluationVM& VM) const override;

	// The program to execute
	const UE::AnimNext::FEvaluationProgram* Program = nullptr;
};
