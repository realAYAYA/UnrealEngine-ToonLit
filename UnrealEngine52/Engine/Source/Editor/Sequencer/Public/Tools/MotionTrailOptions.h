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

	UPROPERTY(EditAnywhere, Category = Trail);
	bool bShowTrails;

	UPROPERTY(EditAnywhere,  Category = Trail)
	FLinearColor TrailColor;

	UPROPERTY(EditAnywhere, Category = Trail)
	bool bShowFullTrail;

	UPROPERTY(EditAnywhere, Category = Trail, Meta = (ClampMin = "0.0"))
	float TrailThickness;

	UPROPERTY(EditAnywhere, Category = Trail, Meta = (EditCondition = "!bShowFullTrail", ClampMin = "0"))
	int32 FramesBefore;

	UPROPERTY(EditAnywhere, Category = Trail, Meta = (EditCondition = "!bShowFullTrail", ClampMin = "0"))
	int32 FramesAfter;

	UPROPERTY(EditAnywhere, Category = Trail, Meta = (ClampMin = "1.0"))
	int32 EvalsPerFrame;
	
	UPROPERTY(EditAnywhere, Category = Keys)
	bool bShowKeys;

	UPROPERTY(EditAnywhere, Category = Keys)
	bool bShowFrameNumber;

	UPROPERTY(EditAnywhere, Category = Keys)
	FLinearColor KeyColor;

	UPROPERTY(EditAnywhere, Category = Keys, Meta = (ClampMin = "0.0"))
	double KeySize;

	UPROPERTY(EditAnywhere, Category = Marks)
	bool bShowMarks;

	UPROPERTY(EditAnywhere, Category = Marks)
	FLinearColor MarkColor;

	UPROPERTY(EditAnywhere, Category = Marks, Meta = (ClampMin = "0.0"))
	double MarkSize;

	UPROPERTY(EditAnywhere, Category = Marks)
	bool bLockMarksToFrames;

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