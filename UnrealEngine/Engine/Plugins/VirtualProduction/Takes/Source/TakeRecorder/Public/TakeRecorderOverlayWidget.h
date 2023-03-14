// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "TakeRecorderOverlayWidget.generated.h"

class UTakeRecorder;

UCLASS(Blueprintable, BlueprintType)
class TAKERECORDER_API UTakeRecorderOverlayWidget : public UUserWidget
{
public:

	GENERATED_BODY()

	UTakeRecorderOverlayWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * Set the recorder that this overlay is reflecting
	 */
	void SetRecorder(UTakeRecorder* InRecorder)
	{
		Recorder = InRecorder;
	}

protected:

	/** The recorder that this overlay is reflecting */
	UPROPERTY(BlueprintReadOnly, Category="Take Recorder")
	TObjectPtr<UTakeRecorder> Recorder;
};