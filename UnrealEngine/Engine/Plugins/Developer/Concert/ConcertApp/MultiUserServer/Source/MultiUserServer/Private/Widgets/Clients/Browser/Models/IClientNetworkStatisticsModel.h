// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FConcertSessionClientInfo;
struct FConcertSessionInfo;
struct FMessageAddress;
struct FMessageTransportStatistics;
struct FTransferStatistics;

namespace UE::MultiUserServer
{
	/** Decouples the UI from the server functions. */
	class IClientNetworkStatisticsModel
	{
	public:
		
		virtual TOptional<FMessageTransportStatistics> GetLatestNetworkStatistics(const FMessageAddress& ClientAddress) const = 0;
		/** Whether this client is currently reachable */
		virtual bool IsOnline(const FMessageAddress& ClientAddress) const = 0;

		virtual ~IClientNetworkStatisticsModel() = default;
	};

	// Should move IClientNetworkStatisticsModel into the this namespace for more cohesion
	namespace NetworkStatistics
	{
		FString FormatIPv4AsString(const TOptional<FMessageTransportStatistics>& Stats);
		
		FString FormatTotalBytesSent(const FMessageTransportStatistics& Stats);
		FString FormatTotalBytesReceived(const FMessageTransportStatistics& Stats);
		FString FormatAverageRTT(const FMessageTransportStatistics& Stats);
		FString FormatBytesInflight(const FMessageTransportStatistics& Stats);
		FString FormatTotalBytesLost(const FMessageTransportStatistics& Stats);
	}
}


