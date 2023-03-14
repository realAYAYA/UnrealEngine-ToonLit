// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Slider.h"
#include "SAnalogSlider.h"
#include "CommonInputSubsystem.h"

#include "Types/NavigationMetaData.h"

#include "AnalogSlider.generated.h"

class SAnalogSlider;

/**
 * A simple widget that shows a sliding bar with a handle that allows you to control the value between 0..1.
 *
 * * No Children
 */
UCLASS()
class COMMONUI_API UAnalogSlider : public USlider
{
	GENERATED_UCLASS_BODY()

public:
	/** Called when the value is changed by slider or typing. */
	UPROPERTY(BlueprintAssignable, Category = "Widget Event")
	FOnFloatValueChangedEvent OnAnalogCapture;

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface
	
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	
	void HandleOnAnalogCapture(float InValue);

	void HandleInputMethodChanged(ECommonInputType CurrentInputType);

protected:
	TSharedPtr<SAnalogSlider> MyAnalogSlider;
};
