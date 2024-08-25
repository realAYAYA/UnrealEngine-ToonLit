// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertSharedSlate
{
	class IEditableMultiReplicationStreamModel;
	class IReplicationStreamEditor;
	
	/**
	 * Widget which edits multiple replication stream.
	 * @see ReplicationWidgetFactories.h
	 */
	class CONCERTSHAREDSLATE_API IMultiReplicationStreamEditor : public SCompoundWidget
	{
	public:

		/** @return The widget drawing the consolidated model. */
		virtual IReplicationStreamEditor& GetEditorBase() const = 0;

		/** @return The source of the sub-streams */
		virtual IEditableMultiReplicationStreamModel& GetMultiStreamModel() const = 0;

		/** @return Gets a model that combines all streams into one. */
		virtual IReplicationStreamModel& GetConsolidatedModel() const = 0;
	};
}
