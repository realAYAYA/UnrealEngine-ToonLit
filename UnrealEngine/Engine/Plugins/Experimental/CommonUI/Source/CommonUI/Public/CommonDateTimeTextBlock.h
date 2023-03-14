// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonTextBlock.h"
#include "CommonDateTimeTextBlock.generated.h"

UCLASS(BlueprintType)
class COMMONUI_API UCommonDateTimeTextBlock : public UCommonTextBlock
{
	GENERATED_BODY()

public:
	UCommonDateTimeTextBlock(const FObjectInitializer& ObjectInitializer);

	DECLARE_EVENT(UCommonDateTimeTextBlock, FOnTimeCountDownCompletion);
	FOnTimeCountDownCompletion& OnTimeCountDownCompletion() const { return OnTimeCountDownCompletionEvent; }

#if WITH_EDITOR
	const FText GetPaletteCategory() override;
#endif // WITH_EDITOR

	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintCallable, Category = "DateTime Text Block")
	void SetDateTimeValue(const FDateTime InDateTime, bool bShowAsCountdown, float InRefreshDelay = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "DateTime Text Block")
	void SetTimespanValue(const FTimespan InTimespan);

	UFUNCTION(BlueprintCallable, Category = "DateTime Text Block")
	void SetCountDownCompletionText(const FText InCompletionText);

	UFUNCTION(BlueprintCallable, Category = "DateTime Text Block")
	FDateTime GetDateTime() const;

protected:
	
	void UpdateUnderlyingText();

	virtual bool ShouldClearTimer(const FTimespan& TimeRemaining) const;

	virtual TOptional<FText> FormatTimespan(const FTimespan& InTimespan) const;
	virtual TOptional<FText> FormatDateTime(const FDateTime& InDateTime) const;

	int32 GetLastDaysCount() const { return LastDaysCount; }
	int32 GetLastHoursCount() const { return LastHoursCount; }

private:

	// Timer handle for timer-based ticking based on InterpolationUpdateInterval.
	FTimerHandle TimerTickHandle;
	float LastTimerTickTime;

	FDateTime DateTime;
	bool bShowAsCountdown;
	int32 LastDaysCount;
	int32 LastHoursCount;

	bool bUseCountdownCompletionText;
	FText CountdownCompletionText;

	mutable FOnTimeCountDownCompletion OnTimeCountDownCompletionEvent;

	void TimerTick();
};