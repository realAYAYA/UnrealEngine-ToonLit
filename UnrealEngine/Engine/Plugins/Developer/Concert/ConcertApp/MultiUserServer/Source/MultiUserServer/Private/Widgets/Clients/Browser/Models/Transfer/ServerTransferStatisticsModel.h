// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TransferStatisticsModel.h"
#include "Widgets/Clients/Browser/Models/ITransferStatisticsModel.h"

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

