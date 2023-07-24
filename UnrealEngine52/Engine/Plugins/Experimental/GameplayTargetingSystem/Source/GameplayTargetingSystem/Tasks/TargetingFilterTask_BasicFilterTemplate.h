// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TargetingTask.h"
#include "UObject/Object.h"

#include "TargetingFilterTask_BasicFilterTemplate.generated.h"

class AActor;
class UTargetingSubsystem;
struct FTargetingDebugInfo;
struct FTargetingDefaultResultData;
struct FTargetingRequestHandle;


/**
*	@class UTargetingFilterTask_BasicFilterTemplate
*
*	A base class that has a basic setup struct that a majority of filtering tasks
*	will find convenient.
*/
UCLASS(Abstract)
class TARGETINGSYSTEM_API UTargetingFilterTask_BasicFilterTemplate : public UTargetingTask
{
	GENERATED_BODY()

public:
	UTargetingFilterTask_BasicFilterTemplate(const FObjectInitializer& ObjectInitializer);

private:
	/** Evaluation function called by derived classes to process the targeting request */
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;

protected:
	/** Called against every target data to determine if the target should be filtered out */
	virtual bool ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const;

	/** Debug Helper Methods */
#if ENABLE_DRAW_DEBUG
private:
	virtual void DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const override;
	void AddFilteredTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const;
	void ResetFilteredTarget(const FTargetingRequestHandle& TargetingHandle) const;
#endif // ENABLE_DRAW_DEBUG
	/** ~Debug Helper Methods */
};

