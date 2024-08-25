// Copyright Epic Games, Inc. All Rights Reserved.

#include "SClientToolbar.h"

#include "Widgets/ActiveSession/Replication/Client/SReplicationStatus.h"

#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

namespace UE::MultiUserClient
{
	void SClientToolbar::Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 0.f)
			[
				InArgs._ViewSelectionArea.Widget
			]
		];
	}
}
