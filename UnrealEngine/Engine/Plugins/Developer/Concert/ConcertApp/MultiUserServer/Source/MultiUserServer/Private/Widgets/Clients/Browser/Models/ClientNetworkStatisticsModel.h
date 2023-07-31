// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IClientNetworkStatisticsModel.h"

namespace UE::MultiUserServer
{
	/** Synchronizes the network statistics (the statistics are updated async). */
	class FClientNetworkStatisticsModel : public IClientNetworkStatisticsModel
	{
	public:

		//~ Begin IClientNetworkStatisticsModel Interface
		virtual TOptional<FMessageTransportStatistics> GetLatestNetworkStatistics(const FMessageAddress& ClientAddress) const override;
		virtual bool IsOnline(const FMessageAddress& ClientAddress) const override;
		//~ End IClientNetworkStatisticsModel Interface
	};
}

