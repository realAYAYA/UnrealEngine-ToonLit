// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWebAPIMessageLog.h"

#include "MessageLogModule.h"
#include "WebAPIMessageLog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"

void SWebAPIMessageLog::Construct(const FArguments& InArgs, const TSharedRef<FWebAPIMessageLog>& InMessageLog)
{
	// Create widget from MessageLog module
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	LogListingWidget = MessageLogModule.CreateLogListingWidget(InMessageLog->GetMessageLogListing().ToSharedRef());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Menu.Background"))
		.Padding(2.f)
		[
			LogListingWidget.ToSharedRef()
		]
	];
}
