// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonButtonBase.h"


#include "Input/NavigationReply.h"
#include "CommonRotator.generated.h"

class UCommonTextBlock;

UENUM(BlueprintType)
enum class ERotatorDirection : uint8
{
	Right,
	Left,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRotated, int32, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRotatedWithDirection, int32, Value, ERotatorDirection, RotatorDir);

/**
* A button that can rotate between given text labels.
*/
UCLASS(meta = (DisableNativeTick))
class COMMONUI_API UCommonRotator : public UCommonButtonBase
{
	GENERATED_UCLASS_BODY()

public:

	virtual bool Initialize() override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply);

	/** Handle and use controller navigation to rotate text */
	FNavigationDelegate OnNavigation;
	TSharedPtr<SWidget> HandleNavigation(EUINavigation UINavigation);

	/** Set the array of texts available */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void PopulateTextLabels(TArray<FText> Labels);

	/** Gets the current text value of the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	FText GetSelectedText() const;

	/** Sets the current value of the slider. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	virtual void SetSelectedItem(int32 InValue);

	/** Gets the current selected index */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	/** Shift the current text left. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void ShiftTextLeft();

	/** Shift the current text right. */
	UFUNCTION(BlueprintCallable, Category = "Behavior")
	void ShiftTextRight();

public:

	/** Called when the Selected state of this button changes. Provides the direction of rotation. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnRotatedWithDirection OnRotatedWithDirection;

	/** Called when the Selected state of this button changes */
	UE_DEPRECATED(5.4, "OnRotated is deprecated, please use OnRotatedWithDirection instead.")
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnRotated OnRotated;

	DECLARE_EVENT_TwoParams(UCommonRotator, FRotationEvent, int32 /*Value*/, bool /*bUserInitiated*/);
	FRotationEvent OnRotatedEvent;

protected:
	void ShiftTextLeftInternal(bool bFromNavigation);
	void ShiftTextRightInternal(bool bFromNavigation);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = Events, meta = (DisplayName = "On Options Populated"))
	void BP_OnOptionsPopulated(int32 Count);

	UFUNCTION(BlueprintImplementableEvent, Category = Events, meta = (DisplayName = "On Options Selected"))
	void BP_OnOptionSelected(int32 Index);

protected:

	/** The displayed text */
	UPROPERTY(BlueprintReadOnly, Category = CommonRotator, Meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> MyText;

	// Holds the array of display texts
	TArray<FText> TextLabels;

	/** The index of the current text item */
	int32 SelectedIndex;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CommonBorder.h"
#include "CommonTextBlock.h"
#include "CommonUITypes.h"
#include "CommonUserWidget.h"
#include "Types/NavigationMetaData.h"
#endif
