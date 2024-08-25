// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"

class SMenuAnchor;

namespace UE::Sequencer
{

class IOutlinerExtension;

class SEQUENCERCORE_API SOutlinerColumnButton : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SOutlinerColumnButton) : _IsFocusable(false) {}

		SLATE_ATTRIBUTE(const FSlateBrush*, Image)

		SLATE_ATTRIBUTE(bool, IsRowHovered)

		SLATE_ARGUMENT(bool, IsFocusable)

		SLATE_EVENT(FOnClicked, OnClicked)

		SLATE_EVENT(FOnGetContent, OnGetMenuContent)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	FSlateColor GetColorAndOpacity() const;

	FReply ToggleMenu();

private:

	TSharedPtr<SMenuAnchor> MenuAnchor;
	TAttribute<bool> IsRowHovered;
};

} // namespace UE::Sequencer