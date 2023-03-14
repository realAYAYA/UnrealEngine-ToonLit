// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"

class SWidget;

class SDerivedDataCacheSettingsDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDerivedDataCacheSettingsDialog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	TSharedRef<SWidget> GetGridPanel();

	EActiveTimerReturnType UpdateGridPanels(double InCurrentTime, float InDeltaTime);

	SVerticalBox::FSlot* GridSlot = nullptr;

	FText GetSetting1Text() const;
	FText GetSetting2Text() const;

	void OnSetting1TextCommited(const FText& InText, ETextCommit::Type InCommitType) const;
	void OnSetting2TextCommited(const FText& InText, ETextCommit::Type InCommitType) const;
	void OnNotifcationsEnabledCheckboxChanged(ECheckBoxState NewCheckboxState);

	EVisibility GetThrobberVisibility() const;
	bool IsDerivedDataCacheEnabled() const;
	bool IsAcceptSettingsEnabled() const;
	ECheckBoxState AreNotificationsEnabled() const;
	
	FReply OnAcceptSettings();	
	FReply OnDisableDerivedDataCache();

};
