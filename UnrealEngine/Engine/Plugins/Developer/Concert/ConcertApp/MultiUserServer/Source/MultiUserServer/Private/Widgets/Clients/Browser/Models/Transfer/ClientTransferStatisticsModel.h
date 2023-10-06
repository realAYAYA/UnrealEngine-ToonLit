// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "TransferStatisticsModel.h"

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

