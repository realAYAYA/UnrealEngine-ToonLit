// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertGroupTreeItem.h"
#include "ConcertSessionTreeItem.h"

/** Groups several active sessions */
class CONCERTSHAREDSLATE_API FConcertActiveGroupTreeItem : public FConcertGroupTreeItem
{
	using Super = FConcertGroupTreeItem;
public:
	
	FConcertActiveGroupTreeItem(FGetSessions GetSessionsFunc)
		: Super(MoveTemp(GetSessionsFunc), { FConcertSessionTreeItem::EType::ActiveSession, FConcertSessionTreeItem::EType::NewSession, FConcertSessionTreeItem::EType::SaveSession })
	{}
};