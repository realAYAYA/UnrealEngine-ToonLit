// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWindow.h"

class FContextualAnimViewModel;
class FStructOnScope;

/** Dialog for adding a new AnimSet */
class SContextualAnimNewAnimSetDialog : public SWindow
{
public:

	SLATE_BEGIN_ARGS(SContextualAnimNewAnimSetDialog){}
	SLATE_END_ARGS()

	SContextualAnimNewAnimSetDialog() 
		: UserResponse(EAppReturnType::Cancel)
	{
	}

	void Construct(const FArguments& InArgs, const TSharedRef<FContextualAnimViewModel>& InViewModel);

	EAppReturnType::Type Show();

protected:

	TWeakPtr<FContextualAnimViewModel> WeakViewModel;

	EAppReturnType::Type UserResponse;

	TSharedPtr<FStructOnScope> WidgetStruct;

	FReply OnButtonClick(EAppReturnType::Type ButtonID);
};
