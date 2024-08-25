// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/ClientName/SLocalClientName.h"

#include "IConcertClient.h"

#include "Widgets/ClientName/SClientName.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SLocalClientName"

namespace UE::ConcertClientSharedSlate
{
	void SLocalClientName::Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient)
	{
		ChildSlot
		[
			SNew(SClientName)
			.ClientInfo_Lambda([InClient](){ return &InClient->GetClientInfo(); })
			.DisplayAsLocalClient(true)
			.HighlightText(InArgs._HighlightText)
			.Font(InArgs._Font)
		];
	}
}

#undef LOCTEXT_NAMESPACE