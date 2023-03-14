// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "OpenColorIOColorSpace.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"


class FViewportClient;
class SComboButton;
class UOpenColorIOConfiguration;

DECLARE_DELEGATE_TwoParams(FOnColorSpaceChanged, const FOpenColorIOColorSpace& /*ColorSpace*/, const FOpenColorIODisplayView& /*DisplayView*/);

class SOpenColorIOColorSpacePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOpenColorIOColorSpacePicker) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UOpenColorIOConfiguration>, Config)
		SLATE_ARGUMENT(FOpenColorIOColorSpace, InitialColorSpace)
		SLATE_ARGUMENT(FOpenColorIOColorSpace, RestrictedColor)
		SLATE_ARGUMENT(FOpenColorIODisplayView, InitialDisplayView)
		SLATE_ARGUMENT(bool, IsDestination)
		SLATE_EVENT(FOnColorSpaceChanged, OnColorSpaceChanged)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	/** Update current configuration asset for this picker */
	void SetConfiguration(TWeakObjectPtr<UOpenColorIOConfiguration> NewConfiguration);

	/** Update restricted color space for this picker */
	void SetRestrictedColorSpace(const FOpenColorIOColorSpace& RestrictedColorSpace);

protected:
	
	/** Called when a selection has been made */
	void SetCurrentColorSpace(const FOpenColorIOColorSpace& NewColorSpace);
	void SetCurrentDisplayView(const FOpenColorIODisplayView& NewDisplayView);

	/** Handles color space list menu creation */
	TSharedRef<SWidget> HandleColorSpaceComboButtonMenuContent();
	
	/** Reset to default triggered in UI */
	FReply OnResetToDefault();

	/** Whether or not ResetToDefault button should be shown */
	EVisibility ShouldShowResetToDefaultButton() const;

protected:
	TSharedPtr<SComboButton> SelectionButton;
	TWeakObjectPtr<UOpenColorIOConfiguration> Configuration;
	FOpenColorIOColorSpace ColorSpaceSelection;
	FOpenColorIOColorSpace RestrictedColorSpace;
	FOpenColorIODisplayView DisplayViewSelection;
	FOnColorSpaceChanged OnColorSpaceChanged;
	bool bIsDestination = false;
};
