// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TargetingTask.h"

#include "TargetingSortTask_Base.generated.h"

/** 
*	@class TargetingSortTask_Base
*
*	A base class that has the basic setup for a sort task.
*/
UCLASS(Abstract)
class TARGETINGSYSTEM_API UTargetingSortTask_Base : public UTargetingTask
{
    GENERATED_BODY()

public:
	UTargetingSortTask_Base(const FObjectInitializer& ObjectInitializer);

protected:
	/** Called on every target to get a Score for sorting. This score will be added to the Score float in FTargetingDefaultResultData */
	virtual float GetScoreForTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const;

private:
	/** Evaluation function called by derived classes to process the targeting request */
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Target Sorting | Data")
	uint8 bAscending : 1;

	/** Debug Helper Methods */
#if ENABLE_DRAW_DEBUG
private:
	virtual void DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const override;
	void BuildPreSortDebugString(const FTargetingRequestHandle& TargetingHandle, const TArray<FTargetingDefaultResultData>& TargetResults) const;
	void BuildPostSortDebugString(const FTargetingRequestHandle& TargetingHandle, const TArray<FTargetingDefaultResultData>& TargetResults) const;
	void ResetSortDebugStrings(const FTargetingRequestHandle& TargetingHandle) const;
#endif // ENABLE_DRAW_DEBUG
	/** ~Debug Helper Methods */
};
