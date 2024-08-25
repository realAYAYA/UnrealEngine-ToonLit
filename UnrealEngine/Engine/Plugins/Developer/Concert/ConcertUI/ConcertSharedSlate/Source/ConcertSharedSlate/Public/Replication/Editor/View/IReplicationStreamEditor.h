// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IReplicationStreamViewer.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertSharedSlate
{
	/**
	 * Widget which edits replication stream.
	 * @see ReplicationWidgetFactories.h
	 */
	class CONCERTSHAREDSLATE_API IReplicationStreamEditor : public IReplicationStreamViewer
	{};
}