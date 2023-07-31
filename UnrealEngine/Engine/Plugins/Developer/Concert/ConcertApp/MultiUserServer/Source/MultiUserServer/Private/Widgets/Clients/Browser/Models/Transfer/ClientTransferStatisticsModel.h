// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMessageContext.h"
#include "TransferStatisticsModel.h"
#include "Widgets/Clients/Browser/Models/ITransferStatisticsModel.h"

namespace UE::MultiUserServer
{
	class FClientTransferStatisticsModel : public FTransferStatisticsModelBase
	{
	public:
		
		FClientTransferStatisticsModel(const FMessageAddress& ClientAddress);

	protected:
		
		static bool ShouldIncludeOutboundStat(const FOutboundTransferStatistics& Item, const FMessageAddress ClientAddress);
		static bool ShouldIncludeInboundStat(const FInboundTransferStatistics& Item, const FMessageAddress ClientAddress);
	};
}

