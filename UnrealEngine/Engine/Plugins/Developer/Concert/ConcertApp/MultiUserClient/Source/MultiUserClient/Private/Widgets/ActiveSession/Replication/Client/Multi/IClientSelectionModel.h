// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Templates/Function.h"

enum class EBreakBehavior : uint8;

namespace UE::MultiUserClient
{
	class FReplicationClient;
	
	/** Abstracts the concept of a client selection. */
	class IClientSelectionModel
	{
	public:

		/** Iterates through every client in the selection. */
		virtual void ForEachSelectedClient(TFunctionRef<EBreakBehavior(FReplicationClient&)> ProcessClient) const = 0;

		/** @return Whether the client is selcted. */
		virtual bool ContainsClient(const FGuid& ClientId) const = 0;

		DECLARE_MULTICAST_DELEGATE(FOnSelectionChanged);
		/** Called when the clients ForEachSelectedClient enumerates has changed. */
		virtual FOnSelectionChanged& OnSelectionChanged() = 0;
		
		virtual ~IClientSelectionModel() = default;
	};

}
