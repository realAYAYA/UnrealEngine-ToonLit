// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VisualLoggerRenderingActorBase.h"
#include "VisualLoggerRenderingActor.generated.h"

// enable adding some hard coded shapes to the VisualLoggerRenderingActor for testing
#define VLOG_TEST_DEBUG_RENDERING 0

class UPrimitiveComponent;
struct FVisualLoggerDBRow;

/**
*	Transient actor used to draw visual logger data on level
*/

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, class AActor*);

UCLASS(config = Engine, NotBlueprintable, Transient, notplaceable, AdvancedClassDisplay)
class LOGVISUALIZER_API AVisualLoggerRenderingActor : public AVisualLoggerRenderingActorBase 
{
public:
	GENERATED_UCLASS_BODY()
	virtual ~AVisualLoggerRenderingActor();
	void ResetRendering();
	void ObjectVisibilityChanged(const FName& RowName);
	void ObjectSelectionChanged(const TArray<FName>& Selection);
	void OnItemSelectionChanged(const FVisualLoggerDBRow& BDRow, int32 ItemIndex);

	virtual void IterateDebugShapes(TFunction<void(const FTimelineDebugShapes&) > Callback) override;
	virtual bool MatchCategoryFilters(const FName& CategoryName, ELogVerbosity::Type Verbosity) const override;
private:
	void OnFiltersChanged();

	TArray<FName> CachedRowSelection;
	TMap<FName, FTimelineDebugShapes> DebugShapesPerRow;

#if VLOG_TEST_DEBUG_RENDERING
	void AddDebugRendering();
	FTimelineDebugShapes TestDebugShapes;
#endif
};
