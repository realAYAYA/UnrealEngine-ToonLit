// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/ClientName/SRemoteClientName.h"

#include "IConcertClient.h"
#include "Widgets/ClientName/SClientName.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SLocalClientName"

namespace UE::ConcertClientSharedSlate
{
	void SRemoteClientName::Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient)
	{
		Client = MoveTemp(InClient);
		ClientEndpointIdAttribute = InArgs._ClientEndpointId;
		check(ClientEndpointIdAttribute.IsSet() || ClientEndpointIdAttribute.IsBound());
		
		ChildSlot
		[
			SNew(SClientName)
			.ClientInfo(this, &SRemoteClientName::GetClientInfo)
			.HighlightText(InArgs._HighlightText)
			.Font(InArgs._Font)
		];
	}

	const FConcertClientInfo* SRemoteClientName::GetClientInfo() const
	{
		if (const TSharedPtr<IConcertClientSession> ConnectedSession = Client->GetCurrentSession())
		{
			ConnectedSession->FindSessionClient(ClientEndpointIdAttribute.Get(), LastKnownClientInfo);
		}

		return &LastKnownClientInfo.ClientInfo;
	}
}

#undef LOCTEXT_NAMESPACE