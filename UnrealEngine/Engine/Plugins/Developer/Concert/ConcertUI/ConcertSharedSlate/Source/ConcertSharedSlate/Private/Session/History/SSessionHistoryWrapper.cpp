// Copyright Epic Games, Inc. All Rights Reserved.

#include "Session/History/SSessionHistoryWrapper.h"

void SSessionHistoryWrapper::Construct(const FArguments& InArgs, TSharedRef<FAbstractSessionHistoryController> InController)
{
	Controller = InController;
	ChildSlot
	[
		Controller->GetSessionHistory()
	];
}