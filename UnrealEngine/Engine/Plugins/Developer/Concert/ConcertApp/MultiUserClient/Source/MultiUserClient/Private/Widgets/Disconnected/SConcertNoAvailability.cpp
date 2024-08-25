// Copyright Epic Games, Inc. All Rights Reserved.

#include "SConcertNoAvailability.h"

#include "Layout/Visibility.h"
#include "SConcertDiscovery.h"

void SConcertNoAvailability::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		// Reuse this panel, but only show the message.
		SNew(SConcertDiscovery) 
			.Text(InArgs._Text)
			.ThrobberVisibility(EVisibility::Collapsed)
			.ButtonVisibility(EVisibility::Collapsed)
	];
}
