// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TransferStatisticsModel.h"

namespace UE::MultiUserServer
{
	class FServerTransferStatisticsModel : public FTransferStatisticsModelBase
	{
	public:

		FServerTransferStatisticsModel()
			:
			FTransferStatisticsModelBase(
			TClientTransferStatTracker<FOutboundTransferStatistics>::FShouldInclude::CreateLambda([](const auto&){ return true; }),
			TClientTransferStatTracker<FInboundTransferStatistics>::FShouldInclude::CreateLambda([](const auto& ){ return true; })
			)
		{}
	};
}

