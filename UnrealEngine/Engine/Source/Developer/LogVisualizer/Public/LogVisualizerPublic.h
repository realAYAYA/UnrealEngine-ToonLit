// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

/* Dependencies
*****************************************************************************/
#include "CoreMinimal.h"

class FText;
struct FGeometry;
struct FKeyEvent;
class FReply;
class FString;
class UWorld;
class UObject;

DECLARE_DELEGATE_OneParam(FOnFiltersSearchChanged, const FText&);

//DECLARE_DELEGATE(FOnFiltersChanged);
DECLARE_MULTICAST_DELEGATE(FOnFiltersChanged);

DECLARE_DELEGATE_ThreeParams(FOnLogLineSelectionChanged, TSharedPtr<struct FLogEntryItem> /*SelectedItem*/, int64 /*UserData*/, FName /*TagName*/);
DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnKeyboardEvent, const FGeometry& /*MyGeometry*/, const FKeyEvent& /*InKeyEvent*/);
DECLARE_DELEGATE_RetVal(float, FGetAnimationOutlinerFillPercentageFunc);

struct FVisualLoggerEvents
{
	FOnFiltersChanged OnFiltersChanged;
	FOnLogLineSelectionChanged OnLogLineSelectionChanged;
	FOnKeyboardEvent OnKeyboardEvent;
	FGetAnimationOutlinerFillPercentageFunc GetAnimationOutlinerFillPercentageFunc;
};

class FVisualLoggerTimeSliderController;
struct LOGVISUALIZER_API FLogVisualizer
{
	/** LogVisualizer interface*/
	void Reset();

	FLinearColor GetColorForCategory(int32 Index) const;
	FLinearColor GetColorForCategory(const FString& InFilterName) const;
	UWorld* GetWorld(UObject* OptionalObject = nullptr);
	FVisualLoggerEvents& GetEvents() { return VisualLoggerEvents; }

	void SetCurrentVisualizer(TSharedPtr<class SVisualLogger> Visualizer) { CurrentVisualizer = Visualizer; }

	void SetAnimationOutlinerFillPercentage(float FillPercentage) { AnimationOutlinerFillPercentage = FillPercentage; }
	float GetAnimationOutlinerFillPercentage()
	{
		if (VisualLoggerEvents.GetAnimationOutlinerFillPercentageFunc.IsBound())
		{
			SetAnimationOutlinerFillPercentage(VisualLoggerEvents.GetAnimationOutlinerFillPercentageFunc.Execute());
		}
		return AnimationOutlinerFillPercentage;
	}

	int32 GetNextItem(FName RowName, int32 MoveDistance = 1);
	int32 GetPreviousItem(FName RowName, int32 MoveDistance = 1);

	// @todo: This function currently doesn't do anything!
	void GotoNextItem(FName RowName, int32 MoveDistance = 1);
	// @todo: This function currently doesn't do anything!
	void GotoPreviousItem(FName RowName, int32 MoveDistance = 1);
	void GotoFirstItem(FName RowName);
	void GotoLastItem(FName RowName);

	void UpdateCameraPosition(FName Rowname, int32 ItemIndes);

	void SeekToTime(float Time);

	/** Static access */
	static void Initialize();
	static void Shutdown();
	static FLogVisualizer& Get();

protected:
	TSharedPtr<FVisualLoggerTimeSliderController> GetTimeSliderController() { return TimeSliderController; }

	static TSharedPtr< struct FLogVisualizer > StaticInstance;

	TSharedPtr<FVisualLoggerTimeSliderController> TimeSliderController;
	FVisualLoggerEvents VisualLoggerEvents;
	TWeakPtr<class SVisualLogger> CurrentVisualizer;
	float AnimationOutlinerFillPercentage;

	friend class SVisualLoggerViewer;
	friend class SVisualLoggerView;
	friend class SVisualLogger;
	friend struct FVisualLoggerCanvasRenderer;
};