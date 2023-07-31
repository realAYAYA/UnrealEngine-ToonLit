// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/SIKRigOutputLog.h"

#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#include "RetargetEditor/IKRetargetEditorController.h"

void SIKRigOutputLog::Construct(
	const FArguments& InArgs,
	const FName& InLogName)
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");

	if (MessageLogModule.IsRegisteredLogListing(InLogName))
	{
		MessageLogListing = MessageLogModule.GetLogListing(InLogName);
	}
	else
	{
		FMessageLogInitializationOptions InitOptions;
		InitOptions.bShowFilters = false;
		InitOptions.bShowPages = false;
		InitOptions.bAllowClear = false;
		InitOptions.bShowInLogWindow = false;
		InitOptions.bDiscardDuplicates = true;
		MessageLogModule.RegisterLogListing(InLogName, FText::FromString("IK Rig Log"), InitOptions);
		MessageLogListing = MessageLogModule.GetLogListing(InLogName);
	}

	OutputLogWidget = MessageLogModule.CreateLogListingWidget( MessageLogListing.ToSharedRef() );
	
	ChildSlot
	[
		SNew(SBox)
		[
			OutputLogWidget.ToSharedRef()
		]
	];
}

void SIKRigOutputLog::ClearLog() const
{
	MessageLogListing.Get()->ClearMessages();
}