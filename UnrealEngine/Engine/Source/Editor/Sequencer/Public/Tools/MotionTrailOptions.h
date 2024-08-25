// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "MotionTrailOptions.generated.h"

//if true still use old motion trails for sequencer objects.
extern SEQUENCER_API TAutoConsoleVariable<bool> CVarUseOldSequencerMotionTrails;


// TODO: option to make tick size proportional to distance from camera to get a sense of perspective and scale
UCLASS(config = EditorPerProjectUserSettings)
class SEQUENCER_API UMotionTrailToolOptions : public UObject
{
	GENERATED_BODY()

public:
	UMotionTrailToolOptions()
		: bShowTrails(false)
		, TrailColor(.22,0.15,1.0)
		, bShowFullTrail(true)
		, TrailThickness(0.0f)
		, FramesBefore(10)
		, FramesAfter(10)
		, EvalsPerFrame(1)
		, bShowKeys(true)
		, bShowFrameNumber(true)
		, KeyColor(1.0, 1.0, 1.0)
		, KeySize(10.0f)
		, bShowMarks(false)
		, MarkColor(0.25,1.0,0.15)
		, MarkSize(5.0)
		, bLockMarksToFrames(true)
		, SecondsPerMark(0.1)
	{}

	/** Whether or not to show motion trails */
	UPROPERTY(EditAnywhere, Category = Trail);
	bool bShowTrails;

	/** The color of the motion trail */
	UPROPERTY(EditAnywhere,  Category = Trail)
	FLinearColor TrailColor;

	/** Whether or not to show the full motion trail */
	UPROPERTY(EditAnywhere, Category = Trail)
	bool bShowFullTrail;

	/* The thickness of the motion trail */
	UPROPERTY(EditAnywhere, Category = Trail, Meta = (ClampMin = "0.0"))
	float TrailThickness;

	/** The number of frames to draw before the start of the trail. Requires not showing the full trail */
	UPROPERTY(EditAnywhere, Category = Trail, Meta = (EditCondition = "!bShowFullTrail", ClampMin = "0"))
	int32 FramesBefore;

	/** The number of frames to draw after the end of the trail. Requires not showing the full trail */
	UPROPERTY(EditAnywhere, Category = Trail, Meta = (EditCondition = "!bShowFullTrail", ClampMin = "0"))
	int32 FramesAfter;

	/** No longer exposed and clamped to 1 The number of evaluations per frame */
	int32 EvalsPerFrame;
	
	/** Whether or not to show keys on the motion trail */
	UPROPERTY(EditAnywhere, Category = Keys)
	bool bShowKeys;

	/** Whether or not to show frame numbers for the keys on the motion trail */
	UPROPERTY(EditAnywhere, Category = Keys)
	bool bShowFrameNumber;

	/** The color of the keys */
	UPROPERTY(EditAnywhere, Category = Keys)
	FLinearColor KeyColor;

	/** The size of the keys */
	UPROPERTY(EditAnywhere, Category = Keys, Meta = (ClampMin = "0.0"))
	double KeySize;

	/** Whether or not to show marks along the motion trail */
	UPROPERTY(EditAnywhere, Category = Marks)
	bool bShowMarks;

	/** The color of the marks */
	UPROPERTY(EditAnywhere, Category = Marks)
	FLinearColor MarkColor;

	/** The size of the marks */
	UPROPERTY(EditAnywhere, Category = Marks, Meta = (ClampMin = "0.0"))
	double MarkSize;

	/** Whether or not to lock the marks to the frames */
	UPROPERTY(EditAnywhere, Category = Marks)
	bool bLockMarksToFrames;

	/** The seconds per mark */
	UPROPERTY(EditAnywhere, Category = Marks, Meta = (EditCondition = "!bLockMarksToFrames", ClampMin = "0.01"))
	double SecondsPerMark;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override
	{
		FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
		OnDisplayPropertyChanged.Broadcast(PropertyName);
	}

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDisplayPropertyChanged, FName);
	FOnDisplayPropertyChanged OnDisplayPropertyChanged;


	static UMotionTrailToolOptions* GetTrailOptions()  { return GetMutableDefault<UMotionTrailToolOptions>(); }


};