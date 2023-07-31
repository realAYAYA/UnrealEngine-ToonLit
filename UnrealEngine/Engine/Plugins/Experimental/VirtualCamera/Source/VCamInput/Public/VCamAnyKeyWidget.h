// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "VCamAnyKeyWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKeySelected, FKey, Key);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnKeySelectionCanceled);

// Base widget class that allows you to wait for any input and then respond to the key pressed
UCLASS(Abstract)
class VCAMINPUT_API UVCamPressAnyKey : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FOnKeySelected OnKeySelected;

	UPROPERTY(BlueprintAssignable)
	FOnKeySelectionCanceled OnKeySelectionCanceled;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	void HandleKeySelected(FKey InKey);
	void HandleKeySelectionCanceled();

	void Dismiss(TFunction<void()> PostDismissCallback);

private:
	bool bKeySelected = false;
	TSharedPtr<class FVCamPressAnyKeyInputProcessor> InputProcessor;

	friend class FVCamPressAnyKeyInputProcessor;
};