// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/WidgetSwitcher.h"
#include "Slate/SCommonAnimatedSwitcher.h"
#include "Components/Widget.h"

#include "CommonAnimatedSwitcher.generated.h"

class SOverlay;
class SSpacer;

/**
 * A widget switcher that activates / deactivates CommonActivatableWidgets, allowing for associated animations to trigger.
 */
UCLASS()
class COMMONUI_API UCommonAnimatedSwitcher : public UWidgetSwitcher
{
	GENERATED_BODY()

public:
	UCommonAnimatedSwitcher(const FObjectInitializer& ObjectInitializer);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

	virtual void SetActiveWidgetIndex(int32 Index) override;
	virtual void SetActiveWidget(UWidget* Widget) override;

	UFUNCTION(BlueprintCallable, Category = "Common Widget Switcher")
    void ActivateNextWidget(bool bCanWrap);

    UFUNCTION(BlueprintCallable, Category = "Common Widget Switcher")
    void ActivatePreviousWidget(bool bCanWrap);

    UFUNCTION(BlueprintCallable, Category = "Common Widget Switcher")
    bool HasWidgets() const;

	UFUNCTION(BlueprintCallable, Category = "Common Widget Switcher")
	void SetDisableTransitionAnimation(bool bDisableAnimation);

	UFUNCTION(BlueprintCallable, Category = "Common Widget Switcher")
	bool IsCurrentlySwitching() const;

	/** Is the switcher playing a transition animation? */
	UFUNCTION(BlueprintCallable, Category = "Common Widget Switcher")
	bool IsTransitionPlaying() const;

protected:
	virtual void HandleSlateActiveIndexChanged(int32 ActiveIndex);

	virtual void HandleSlateIsTransitioningChanged(bool bIsTransitioning);

	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void HandleOutgoingWidget() {};

public:
	/** Fires when the active widget displayed by the switcher changes */
	DECLARE_EVENT_TwoParams(UCommonAnimatedSwitcher, FOnActiveIndexChanged, UWidget*, int32)
	FOnActiveIndexChanged OnActiveWidgetIndexChanged;

	/** Fires when the switcher changes its transition animation state */
	DECLARE_EVENT_OneParam(UCommonAnimatedSwitcher, FOnTransitioningChanged, bool)
	FOnTransitioningChanged OnTransitioningChanged;

protected:
	/** The type of transition to play between widgets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	ECommonSwitcherTransition TransitionType;

	/** The curve function type to apply to the transition animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	ETransitionCurve TransitionCurveType;

	/** The total duration of a single transition between widgets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	float TransitionDuration;

	TSharedPtr<SOverlay> MyOverlay;
	TSharedPtr<SSpacer> MyInputGuard;
	TSharedPtr<SCommonAnimatedSwitcher> MyAnimatedSwitcher;

	/* If set, transition animations will not play */
	bool bInstantTransition = false;

	bool bSetOnce = false;
	bool bCurrentlySwitching = false;

private:
	void SetActiveWidgetIndex_Internal(int32 Index);
};