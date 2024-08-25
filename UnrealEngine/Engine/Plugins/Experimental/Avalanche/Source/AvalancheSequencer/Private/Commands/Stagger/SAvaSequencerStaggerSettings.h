// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequencerStaggerSettings.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Misc/Attribute.h"
#include "Misc/OptionalFwd.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SCompoundWidget.h"

class FReply;
class SWidget;

class SAvaSequencerStaggerSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaSequencerStaggerSettings) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	const FAvaSequencerStaggerSettings& GetSettings() const { return Settings; }

	EAppReturnType::Type GetReturnType() const { return ReturnType; }

	TOptional<int32> GetShiftFrame() const;

	void OnShiftFrameChanged(int32 InNewValue);

	FReply OnReturnButtonClicked(EAppReturnType::Type InReturnType);

private:
	TSharedRef<SCheckBox> ConstructRadioButton(const TAttribute<ECheckBoxState>& InCheckBoxState
		, const FOnCheckStateChanged& InOnCheckStateChanged
		, const TAttribute<FText>& InText
		, const TAttribute<FText>& InToolTipText);
	TSharedRef<SWidget> ConstructStartPositionRadioGroup();
	TSharedRef<SWidget> ConstructOperationPointRadioGroup();

	FReply OnResetToDefaults();

	static FAvaSequencerStaggerSettings Settings;

	EAppReturnType::Type ReturnType = EAppReturnType::Cancel;
};
