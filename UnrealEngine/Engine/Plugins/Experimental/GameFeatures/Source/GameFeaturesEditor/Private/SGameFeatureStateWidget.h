// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureTypesFwd.h"

//////////////////////////////////////////////////////////////////////////
// FGameFeatureDataDetailsCustomization

/**
 * A delegate that is invoked when widgets want to notify a user that they have been clicked.
 * Intended for use by buttons and other button-like widgets.
 */
DECLARE_DELEGATE_OneParam(FOnWidgetChangesGameFeatureState, EGameFeaturePluginState)

class SGameFeatureStateWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGameFeatureStateWidget) {}
		SLATE_ATTRIBUTE(EGameFeaturePluginState, CurrentState)
		SLATE_EVENT(FOnWidgetChangesGameFeatureState, OnStateChanged)
	SLATE_END_ARGS()

public:
	SGameFeatureStateWidget() {}

	void Construct(const FArguments& InArgs);

	static FText GetDisplayNameOfState(EGameFeaturePluginState State);
	static FText GetTooltipOfState(EGameFeaturePluginState StateID);

private:
	FText GetStateStatusDisplay() const;
	FText GetStateStatusDisplayTooltip() const;

private:
	TAttribute<EGameFeaturePluginState> CurrentState;
};
