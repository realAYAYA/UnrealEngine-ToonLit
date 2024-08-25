// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOwnerClientList.h"

#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ActiveSession/Replication/Misc/SNoClients.h"
#include "Widgets/ClientName/SHorizontalClientList.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SOwnerClientList"

namespace UE::MultiUserClient
{
	void SOwnerClientList::Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient, FGlobalAuthorityCache& InAuthorityCache)
	{
		AuthorityCache = &InAuthorityCache;
		GetClientListDelegate = InArgs._GetClientList;
		check(GetClientListDelegate.IsBound());
		
		AuthorityCache->OnCacheChanged().AddSP(this, &SOwnerClientList::OnClientChanged);
		
		ChildSlot
		[
			SAssignNew(ClientList, ConcertClientSharedSlate::SHorizontalClientList, MoveTemp(InClient))
			.HighlightText(InArgs._HighlightText)
			.EmptyListSlot()
			[
				SNew(SBox)
				.HAlign(HAlign_Left)
				.Padding(5.f, 0.f, 0.f, 0.f)
				[
					SNew(SNoClients)
				]
			]
		];

		RefreshList();
	}

	SOwnerClientList::~SOwnerClientList()
	{
		AuthorityCache->OnCacheChanged().RemoveAll(this);
	}

	void SOwnerClientList::OnClientChanged(const FGuid& ChangedClientId)
	{
		RefreshList();
	}
	
	void SOwnerClientList::RefreshList()
	{
		const TArray<FGuid> Clients = GetClientListDelegate.Execute(*AuthorityCache);
		ClientList->RefreshList(Clients);
	}
}

#undef LOCTEXT_NAMESPACE