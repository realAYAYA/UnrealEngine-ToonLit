// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParameterPickerArgs.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/SWindow.h"

class SWrapBox;

namespace UE::AnimNext::Editor
{

class SLinkParametersDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SLinkParametersDialog) {}

	SLATE_EVENT(UE::AnimNext::Editor::FOnFilterParameter, OnFilterParameter)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool ShowModal(TArray<FParameterBindingReference>& OutParameters);

private:
	void HandleSelectionChanged();

	TSharedPtr<SWrapBox> QueuedParametersBox;

	FOnGetParameterBindings OnGetParameterBindings;

	bool bCancelPressed = false;
};

}
